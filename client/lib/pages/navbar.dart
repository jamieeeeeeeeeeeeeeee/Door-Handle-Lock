import 'package:curved_navigation_bar/curved_navigation_bar.dart';
import 'package:flutter/material.dart';
import 'package:image_picker/image_picker.dart';

class NavBar extends StatefulWidget {
  final Function(ImageSource) pickImage;
  final Function(int) changePage;
  const NavBar({super.key, required this.pickImage, required this.changePage});

  @override
  State<NavBar> createState() => _NavBarState();
}

class _NavBarState extends State<NavBar> {
  final _bottomNavigationKey = GlobalKey<CurvedNavigationBarState>();

  @override
  Widget build(BuildContext context) {
    return CurvedNavigationBar(
      key: _bottomNavigationKey,
      index: 1,
      animationDuration: const Duration(milliseconds: 600),
      backgroundColor: Colors.white,
      color: Colors.blueAccent,
      items: const <Widget>[
        IconButton(
            icon: Icon(Icons.photo_camera, size: 30, color: Colors.white),
            onPressed: null //() => _pickImage(ImageSource.camera),
            ),
        Icon(Icons.lock, size: 30, color: Colors.white),
        Icon(Icons.settings, size: 30, color: Colors.white),
      ],
      onTap: (index) {
        if (index == 0) {
          widget.pickImage(ImageSource.gallery);
        } else {
          widget.changePage(index);
        }
      },
    );
  }
}
