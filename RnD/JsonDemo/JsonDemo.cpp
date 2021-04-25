// JsonDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <cassert>
#include <list>

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

    inner->AddString("Str", L"Hello!\nThis is lineeed\nTab\t\t\tXXX\n Энд немного русского языка");
    inner->AddNull("Void");

    // Serialize and print

    size_t actualSize = 0;

    json.Serialize(text, sizeof(text) - 1, actualSize);
    std::string str(text, actualSize);
    
    std::cout << str;
}

void CloneDemo()
{
    Json one, two;
    char text[0x1000] = { 0, };

    // Make one

    Json::Value* outer = one.root.AddObject(nullptr);

    outer->AddInt("a", 1);
    Json::Value* inner = outer->AddObject("inner");
    inner->AddInt("b", 2);

    // Clone

    two.Clone(&one);

    // Show clone

    size_t actualSize = 0;

    two.Serialize(text, sizeof(text) - 1, actualSize);
    std::string str(text, actualSize);

    std::cout << str;
}

void MergeDemo()
{
    Json one, two;
    char text[0x1000] = { 0, };

    // Make one

    Json::Value* outer = one.root.AddObject(nullptr);

    outer->AddInt("a", 1);
    outer->AddInt("b", 2);

    // Make two

    outer = two.root.AddObject(nullptr);

    outer->AddInt("b", 777);

    // Merge

    one.Merge(&two);

    // Show clone

    size_t actualSize = 0;

    one.Serialize(text, sizeof(text) - 1, actualSize);
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

    std::cout << "CloneDemo\n";
    CloneDemo();
    std::cout << "\n";

    std::cout << "MergeDemo\n";
    MergeDemo();
    std::cout << "\n";
}
