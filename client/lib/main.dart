import 'package:flutter/material.dart';
import 'package:client/utils/styles.dart';
import 'package:client/pages/pagehandler.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Design Engineering',
      theme: ThemeData(
        primaryColor: Styles.primaryColour,
      ),
      home: const PageHandler(),
    );
  }
}
