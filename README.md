# Biometric-System
Code for ACCESS CONTROL-BIOMETRIC SYSTEM

Abstract:
This project presents the design and implementation of a multi-modal access control system that provides secure and flexible authentication using fingerprint recognition, RFID cards, and keypad-based password entry. Unlike traditional systems that rely on a single method or require multiple authentication steps, the proposed system allows access through any one method based on user authorization, improving both usability and security.
The system is built around an Arduino Nano microcontroller, which coordinates all hardware components including the MFRC522 RFID module, R307S fingerprint sensor, 3×4 keypad, and 16×2 LCD display. The fingerprint module is used as the primary authentication method for the system owner, while RFID cards provide controlled access for other users. The password mechanism serves as a secure fallback and is also used for administrative control.
User credentials such as passwords and RFID card data are stored in EEPROM for persistence, while fingerprint templates are stored within the sensor module. The system also includes an admin mode for managing operations such as user enrollment, credential modification, and system reset.
The proposed system achieves a balance between security, flexibility, and ease of use, making it suitable for applications in homes, offices, and restricted environments.

