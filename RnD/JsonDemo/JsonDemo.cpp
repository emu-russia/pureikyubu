// JsonDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <cassert>

#include "../../SRC/Common/Json.h"

void DeserializeDemo()
{
    // Load JSON file

    FILE* f = nullptr;
    fopen_s(&f, "../../Data/DefaultSettings.json", "rb");
    assert(f);

    uint8_t* jsonText = nullptr;        // utf-8
    size_t jsonTextSize = 0;

    fseek(f, 0, SEEK_END);
    jsonTextSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    jsonText = new uint8_t[jsonTextSize];
    assert(jsonText);

    fread(jsonText, 1, jsonTextSize, f);
    fclose(f);

    // Parse

    Json json;

    json.Deserialize(jsonText, jsonTextSize);
    delete[] jsonText;

    // Serialize what happens

    char text[0x1000] = { 0, };
    size_t actualSize = 0;

    json.Serialize(text, sizeof(text) - 1, actualSize);
    std::string str(text, actualSize);

    std::cout << str;
}

void SerializeDemo()
{
    Json json;
    char text[0x1000] = { 0, };

    // Add some values

    Json::Value* outer = json.root.AddObject(nullptr);
    
    outer->AddInt("Int", 12);
    outer->AddBool("Bool", false);

    Json::Value * inner = outer->AddObject("Inner");

    inner->AddString("Str", _T("Hello!\nThis is lineeed\nTab\t\t\tXXX\n Энд немного русского языка"));
    inner->AddNull("Void");

    // Serialize and print

    size_t actualSize = 0;

    json.Serialize(text, sizeof(text) - 1, actualSize);
    std::string str(text, actualSize);
    
    std::cout << str;
}

int main()
{
    std::cout << "Hello JsonDemo!\n";

    std::cout << "DeserializeDemo\n";
    DeserializeDemo();
    std::cout << "\n";

    std::cout << "SerializeDemo\n";
    SerializeDemo();
    std::cout << "\n";
}
