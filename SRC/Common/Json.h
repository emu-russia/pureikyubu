// Json serialization engine. Json is used to store emulator settings, as well as for the JDI system (Json Debug Interface).

#pragma once

class Json
{

public:
	Json(void* text, size_t textSize);
	~Json();

};
