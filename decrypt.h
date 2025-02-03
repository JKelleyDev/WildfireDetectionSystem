#ifndef DECRYPT_H
#define DECRYPT_H

String decryptMessage(String encryptedMessage) {
    int encryptedLength = encryptedMessage.length() / 2;
    byte encryptedBytes[encryptedLength];

    // Convert HEX String back to Byte Array
    for (int i = 0; i < encryptedLength; i++) {
        encryptedBytes[i] = strtol(encryptedMessage.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
    }

    byte aesIV[16];
    byte decryptedBytes[encryptedLength - 16];

    // Extract IV (first 16 bytes)
    memcpy(aesIV, encryptedBytes, 16);

    // Decrypt Ciphertext (rest of the message)
    aes.decrypt(encryptedBytes + 16, encryptedLength - 16, decryptedBytes, aesKey, 128, aesIV);

    // Convert to String
    String decryptedMessage = String((char*)decryptedBytes);
    return decryptedMessage;
}

#endif 

