import 'dart:convert';
import 'dart:io';

import 'package:client/pages/navbar.dart';
import 'package:flutter/material.dart';
import 'package:file_picker/file_picker.dart';
import 'package:image_picker/image_picker.dart';
import 'package:image/image.dart' as img;
import 'package:client/pages/homepage.dart';
import 'package:client/pages/setuppage.dart';
import 'package:client/utils/storage.dart';

class PageHandler extends StatefulWidget {
  const PageHandler({super.key});

  @override
  State<PageHandler> createState() => _PageHandlerState();
}

class _PageHandlerState extends State<PageHandler> {
  int _page = 1;
  int currentStep = 0;

  File? _file;
  Socket? _socket;
  String? ipAddress;

  @override
  void initState() {
    loadIpAddressFromLocalStorage();
    super.initState();
  }

  Future<String> loadIpAddressFromLocalStorage() async {
    final ipAddressFromLocalStorage = await getStringFromLocalStorage('ip');
    setState(() {
      ipAddress = ipAddressFromLocalStorage;
    });
    return ipAddressFromLocalStorage;
  }

  // Navbar functions
  Future<void> pickImage(ImageSource source) async {
    FilePickerResult? result = await FilePicker.platform.pickFiles();
    setState(() {
      _file = result != null ? File(result.files.single.path as String) : null;
    });
    if (result != null) {
      img.Image? image = img.decodeImage(_file!.readAsBytesSync());
      img.Image smallerImage = img.copyResize(image!, width: 320, height: 240);
      List<int> encodedBytes = smallerImage.getBytes();

      ipAddress = ipAddress ?? await getStringFromLocalStorage('ip');
      if (ipAddress == null) {
        return;
      }
      try {
        final socket = await Socket.connect(ipAddress, 9999);
        //socket.add(utf8.encode("IMAGE"));
        
        int n = 320 * 240 * 3;
        int l = encodedBytes.length;
        if (l < n) {
          int m = l % 3;
          n = l - m; // just in case not divisible by 3
        }
        // send r, g, b, x1, x2 (if x > 255), y
        for (int i = 0; i < n; i += 3) {
          int p = i ~/ 3;
          int x = p % (320);
          int x1 = 0;
          if (x > 255) {
            x1 = x - 255;
            x = 255;
          }
          int y = p ~/ (320);
          socket.add([encodedBytes[i], encodedBytes[i + 1], encodedBytes[i + 2], x, x1, y]);
        }

        while (l < n) {
          socket.add([0, 0, 0]);
          l += 3;
        }
      } catch (e) {
        ScaffoldMessenger.of(context).removeCurrentSnackBar();
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('$e'),),
        );
      }
    }
  }

  void changePage(int page) {
    setState(() {
      _page = page;
    });
  }

  // Home page functions
  Future<void> unlockLock(BuildContext context) async {
    ScaffoldMessenger.of(context).removeCurrentSnackBar();
    ScaffoldMessenger.of(context)
        .showSnackBar(const SnackBar(content: Text('Sending Unlock Command')));
    try {
      final socket = await Socket.connect(ipAddress, 9999);
      socket.add(utf8.encode("UNLCK"));
      socket.flush();
    } catch (e) {
      ScaffoldMessenger.of(context).removeCurrentSnackBar();
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Failed to connect')),
      );
    }
  }

  Future<void> sendNetworkInfo(String ssid, String pass) async {
    Socket socket = await Socket.connect('192.168.42.1', 9999);
    String message = '$ssid?$pass';
    socket.add(utf8.encode(message));
  }

  // Getter and setters
  Future<String> getIpAddress() async {
    if (ipAddress == null) {
      return await loadIpAddressFromLocalStorage();
    }
    return ipAddress!;
  }

  void setIpAddress(String ip) {
    ipAddress = ip;
    saveStringToLocalStorage("ip", ip);
    setState(() {
      ipAddress = ip;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      bottomNavigationBar: NavBar(
        pickImage: pickImage,
        changePage: changePage,
      ),
      body: Container(
        color: Colors.white,
        child: Center(
          child: Column(
            children: <Widget>[
              _page == 2
                  ? const SizedBox(width: 0, height: 0)
                  : _file != null
                      ? Padding(
                          padding: const EdgeInsets.only(top: 45.0),
                          child: Image.file(
                            _file as File,
                            width: 300,
                            height: 300,
                          ),
                        ) //If no image, do a container / spacer of size 300x300 instead
                      : Padding(
                          padding: const EdgeInsets.only(top: 45.0),
                          child: SizedBox(
                            width: 300,
                            height: 300.0,
                            // grey border
                            child: DecoratedBox(
                              decoration: BoxDecoration(
                                border: Border.all(color: Colors.grey),
                              ),
                            ),
                          ),
                        ),
              _page == 1
                  ? HomePage(
                      unlockLock: unlockLock,
                      getIpAddress: getIpAddress,
                      setIpAddress: setIpAddress,
                    )
                  : SetupPage(
                      sendNetworkInfo: sendNetworkInfo,
                      getIpAddress: getIpAddress,
                      setIpAddress: setIpAddress),
            ],
          ),
        ),
      ),
    );
  }
}
