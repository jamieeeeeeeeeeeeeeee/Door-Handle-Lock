import 'package:app_settings/app_settings.dart';
import 'package:cupertino_stepper/cupertino_stepper.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

class SetupPage extends StatefulWidget {
  final Function(String, String) sendNetworkInfo;
  final Function() getIpAddress;
  final Function(String) setIpAddress;
  const SetupPage(
      {super.key,
      required this.sendNetworkInfo,
      required this.getIpAddress,
      required this.setIpAddress});

  @override
  State<SetupPage> createState() => _SetupPageState();
}

class _SetupPageState extends State<SetupPage> {
  final _formKey = GlobalKey<FormState>();
  final _ssidController = TextEditingController();
  final _passwordController = TextEditingController();
  final _ipController = TextEditingController();

  int _currentStep = 0;

  String? _detailsErrorMessage;

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
      child: Padding(
        padding: const EdgeInsets.all(1.0),
        //child: Center(
        child: SingleChildScrollView(
          scrollDirection: Axis.vertical,
          physics: const BouncingScrollPhysics(
              decelerationRate: ScrollDecelerationRate.normal),
          child: Padding(
            padding: const EdgeInsets.only(top: 40.0),
            child: Column(children: [
              const Text("Setup Instructions",
                  style: TextStyle(
                      fontSize: 30,
                      fontWeight: FontWeight.bold,
                      color: Colors.black)),
              OrientationBuilder(
                builder: (BuildContext context, Orientation orientation) {
                  switch (orientation) {
                    case Orientation.portrait:
                      return _buildStepper(StepperType.vertical);
                    case Orientation.landscape:
                      return _buildStepper(StepperType.vertical);
                    default:
                      throw UnimplementedError(orientation.toString());
                  }
                },
              ),
            ]),
          ),
        ),
      ),
    );
  }

  void onContinue() {
    if (_currentStep == 1) {
      widget.sendNetworkInfo(_ssidController.text, _passwordController.text);
    } else if (_currentStep == 3) {
      widget.setIpAddress(_ipController.text);
    }
    setState(() {
      _currentStep + 1 != 4 ? _currentStep += 1 : null;
    });
  }

  CupertinoStepper _buildStepper(StepperType type) {
    final canContinue = _currentStep < 4;

    return CupertinoStepper(
      type: type,
      currentStep: _currentStep,
      onStepTapped: (step) => setState(() => _currentStep = step),
      onStepCancel: null,
      onStepContinue: null,
      controlsBuilder: (BuildContext context, ControlsDetails controls) {
        return Row(children: [
          canContinue
              ? CupertinoButton(
                  onPressed: onContinue,
                  child: const Text('Continue'),
                )
              : const SizedBox(width: 0, height: 0),
        ]);
      },
      steps: [
        _buildStep(
          height: 120,
          title: const Text('Step 1'),
          isActive: 0 == _currentStep,
          state: 0 == _currentStep
              ? StepState.editing
              : 0 < _currentStep
                  ? StepState.complete
                  : StepState.indexed,
          subtitle: "Connect to the lock's Wi-Fi network",
          child: Column(
            children: [
              const Text(
                  "Click below to open your settings and connect to the lock's network.\n",
                  style: TextStyle(fontSize: 15)),
              CupertinoButton(
                padding: const EdgeInsets.all(0),
                child: const Text("Door Lock - xx:xx",
                    style:
                        TextStyle(fontSize: 15, fontWeight: FontWeight.bold)),
                onPressed: () {
                  AppSettings.openWIFISettings();
                },
              ),
            ],
          ),
        ),
        _buildStep(
          height: 155,
          title: const Text('Step 2'),
          isActive: 1 == _currentStep,
          state: 1 == _currentStep
              ? StepState.editing
              : 1 < _currentStep
                  ? StepState.complete
                  : StepState.indexed,
          subtitle: "Send your home network details",
          child: Form(
            key: _formKey,
            child: Column(
              //crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Center(
                    child: Text(
                        "Enter your home network details below, and click continue.",
                        style: TextStyle(fontSize: 15))),
                TextFormField(
                  controller: _ssidController,
                  keyboardType: TextInputType.text,
                  decoration: const InputDecoration(
                    labelText: 'Network Name',
                    hintText: '',
                  ),
                ),
                TextFormField(
                  controller: _passwordController,
                  obscureText: false,
                  keyboardType: TextInputType.visiblePassword,
                  decoration: const InputDecoration(
                    labelText: 'Password',
                    hintText: 'Enter your password',
                  ),
                ),
                if (_detailsErrorMessage != null)
                  Text(
                    _detailsErrorMessage!,
                    style: const TextStyle(color: Colors.red),
                  ),
              ],
            ),
          ),
        ),
        _buildStep(
          title: const Text('Step 3'),
          isActive: 2 == _currentStep,
          state: 2 == _currentStep
              ? StepState.editing
              : 2 < _currentStep
                  ? StepState.complete
                  : StepState.indexed,
          subtitle: "Reconnect to your home network",
          child: Column(
            children: [
              const Text("Reconnect your device to your home network.\n",
                  style: TextStyle(fontSize: 15)),
              CupertinoButton(
                padding: const EdgeInsets.all(0),
                child: const Text("Open Settings",
                    style:
                        TextStyle(fontSize: 15, fontWeight: FontWeight.bold)),
                onPressed: () {
                  AppSettings.openWIFISettings();
                },
              ),
            ],
          ),
        ),
        _buildStep(
            title: const Text('Last Step!'),
            isActive: 3 == _currentStep,
            state: 3 == _currentStep
                ? StepState.editing
                : 3 < _currentStep
                    ? StepState.complete
                    : StepState.indexed,
            subtitle: "Enter the IP address displayed on the lock below.",
            child: Form(
              child: TextFormField(
                controller: _ipController,
                keyboardType: TextInputType.text,
                decoration: const InputDecoration(
                  labelText: '192.168.x.x',
                ),
              ),
            )),
      ],
    );
  }

  Step _buildStep({
    required Widget title,
    StepState state = StepState.indexed,
    bool isActive = false,
    String subtitle = '',
    Widget? child,
    double? height,
  }) {
    return Step(
      title: title,
      subtitle: Text(subtitle),
      state: state,
      isActive: isActive,
      content: Container(
        child: child,
      ),
    );
  }
}
