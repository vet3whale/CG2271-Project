#ifndef GEMINI_H
#define GEMINI_H
#include "passwords.h"
#include <Arduino.h>


#define GEMINI_MODEL "gemini-2.5-flash-lite"

void Gemini_Init(void);
String postGemini(const String &prompt);
void vGeminiTask(void *pvParameters);

#endif /* GEMINI_H */
