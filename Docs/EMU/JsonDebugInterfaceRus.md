# Json Debug Interface (JDI)

JDI (произносится как _"Jay-Dai"_) это сервисный и отладочный интерфейс Dolwin, который связывает через единый хаб все компоненты эмулятора.

В инфраструктуру JDI входят следующие сущности:
- Json описание отладочных команд компонента (JDI Node)
- Реализация отладочных команд (инкапсулирована внутри компонента)
- Общий хаб (JDI Hub), через который производится выполнение команд

Основная идея заключается в изоляции внутреннего состояния компонентов эмулятора, с возможностью доступа к ним, используя Json.

Каждый компонент реализует отдельный JDI Node. Все JDI Node доступны через единый хаб (**Debug::Hub**).

## Формат Json JDI Node

Json состоит из двух основных секций: **info** и **can**.

### Секция info

Содержит описание JDI Node (**description**) и заголовок **helpGroup**, который показывается в команде help при выводе списка команд данной JDI Node.

### Секция can

В секции **can** находится описание того, что умеет делать данный JDI Node.

Секция содержит записи с описаниями отладочных команд. Важные свойства:
- **help**: описание команды для справки, вызываемой командой help
- **hints**: дополнительные подсказки, которые выводятся напротив команды
- **args**: количество обязательных аргументов (не считая **args[0]**)
- **usage**: подробная справка по команде, если она была вызывана с недостаточным количеством аргументов
- **output**: информативная подсказка в случае если команда возвращает какие-то данные в виде **Json::Value**
- **internal**: команда не выводится в списке `help`. Используется для внутренних нужд.

Все свойства, кроме свойства **help**, не являются обязательными.

## Отладочная команда

Процедура обратного вызова отладочной команды (_делегата_) выглядит следующим образом:

```c++

typedef Json::Value* (*CmdDelegate)(std::vector<std::string>& args);

```

Команда принимает на вход список аргументов. Аргумент **args[0]** содержит имя самой команды.

Команда может возвращать какое-то **Json::Value** или **nullptr**.

Возвращаемое значение необходимо удалять после использования (delete).

Командам нет необходимости проверять количество аргументов. Эту задачу выполняет JDI Hub (используя свойство "args")

Пример команды:

```c++
	// Mount GC DVD image (GCM)
	static Json::Value* MountIso(std::vector<std::string>& args)
	{
		bool result = MountFile(args[1]);

		if (result)
		{
			DBReport2(DbgChannel::DVD, "Mounted disk image: %s\n", args[1].c_str());
		}
		else
		{
			DBReport2(DbgChannel::Error, "Failed to mount disk image: %s\n", args[1].c_str());
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;
		output->value.AsBool = result;

		return output;
	}
```

## Отладочный вывод

Команды могут выводить текстовую отладочную информацию используя классические методы **DBReport** и **DBReport2**.

DBReport выводит информацию в общий канал. DBReport2 выводит информацию в указанный канал. Список доступных каналов (DbgChannel) находится в заголовке Debugger.h

## Рефлектор

Специальный метод, который регистрирует ассоциативную привязку отладочной команды с её текстовым названием.

Рефлектор используется только один раз при добавлении JDI Node.

Пример рефлектора:

```c++
	void DvdCommandsReflector()
	{
		Debug::Hub.AddCmd("DvdInfo", DvdInfo);
		Debug::Hub.AddCmd("MountIso", MountIso);
		Debug::Hub.AddCmd("OpenLid", OpenLid);
		Debug::Hub.AddCmd("CloseLid", CloseLid);
		Debug::Hub.AddCmd("DvdStats", DvdStats);
		Debug::Hub.AddCmd("MountSDK", MountSDK);
		Debug::Hub.AddCmd("UnmountDvd", UnmountDvd);
		Debug::Hub.AddCmd("DvdSeek", DvdSeek);
		Debug::Hub.AddCmd("DvdRead", DvdRead);
		Debug::Hub.AddCmd("DvdOpenFile", DvdOpenFile);
	}
```

## Добавление JDI Node

- Создать описание Json
- Сделать .cpp модуль с реализацией команд и рефлектором
- Вызывать **Debug::Hub::AddNode**

## Выполнение команд

Используется метод **Debug::Hub::Execute**.
