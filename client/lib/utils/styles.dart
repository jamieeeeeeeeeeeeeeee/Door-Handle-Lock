import 'package:flutter/material.dart';

Color primary = const Color.fromARGB(255, 0, 0, 0);

class Styles {
  static Color primaryColour = primary;
  static Color textColour = const Color(0xff3b3b3b);
  static Color backgroundColour = const Color(0xffeeedf2);
  static Color otherColour = const Color.fromARGB(255, 255, 187, 0);
  static Color orangeColour = const Color.fromARGB(255, 255, 187, 0);
  static TextStyle textStyle = TextStyle(
    color: textColour,
    fontSize: 16,
    fontWeight: FontWeight.w500,
  );
  static TextStyle headlineStyle1 = TextStyle(
    color: textColour,
    fontSize: 26,
    fontWeight: FontWeight.bold,
  );
  static TextStyle headlineStyle2 = TextStyle(
    color: textColour,
    fontSize: 21,
    fontWeight: FontWeight.bold,
  );
  static TextStyle headlineStyle3 = TextStyle(
    color: Colors.grey.shade500,
    fontSize: 18,
    fontWeight: FontWeight.w500,
  );
  static TextStyle headlineStyle4 = TextStyle(
    color: Colors.grey.shade500,
    fontSize: 14,
    fontWeight: FontWeight.w500,
  );
  static TextStyle headlineStyle5 = const TextStyle(
    color: Colors.white,
    fontSize: 12,
    fontWeight: FontWeight.w500,
  );
}
