import 'package:flutter/material.dart';
import 'package:nice_buttons/nice_buttons.dart';

class HomePage extends StatefulWidget {
  final Function(BuildContext) unlockLock;
  final Function() getIpAddress;
  final Function(String) setIpAddress;
  const HomePage(
      {super.key,
      required this.unlockLock,
      required this.getIpAddress,
      required this.setIpAddress});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final _ipController = TextEditingController();
  bool _locking = false;

  @override
  void initState() {
    _loadIP();
    super.initState();
  }

  Future<void> _loadIP() async {
    String ip = await widget.getIpAddress();
    setState(() {
      _ipController.text = ip;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Padding(
            padding: const EdgeInsets.only(left: 32, right: 32, bottom: 8),
            child: TextFormField(
              controller: _ipController,
              keyboardType: TextInputType.text,
              decoration: const InputDecoration(
                labelText: 'Lock IP Address',
              ),
            ),
          ),
          SizedBox(
            child: NiceButtons(
              stretch: false,
              height: 30,
              width: 300,
              borderColor: Colors.green,
              gradientOrientation: GradientOrientation.Horizontal,
              startColor: Colors.green,
              endColor: Colors.greenAccent,
              child: const Text('Save IP Address',
                  style: TextStyle(fontSize: 20, color: Colors.white)),
              onTap: (finish) {
                widget.setIpAddress(_ipController.text);
                finish();
              },
            ),
          ),
          const SizedBox(height: 10),
          SizedBox(
            width: 300,
            height: 100,
            //color: Colors.green,
            child: NiceButtons(
              progress: _locking,
              stretch: false,
              height: 95,
              gradientOrientation: GradientOrientation.Horizontal,
              child: const Text('Unlock Lock',
                  style: TextStyle(fontSize: 30, color: Colors.white)),
              onTap: (finish) {
                if (!_locking) {
                  setState(() {
                    _locking = true;
                  });
                  widget.unlockLock(context);
                }
                setState(() {
                  _locking = false;
                });
                finish();
              },
            ),
          ),
        ],
      ),
    );
  }
}
