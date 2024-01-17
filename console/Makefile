all: app

app: main.cpp console.hpp console.cpp shell.hpp shell.cpp
	g++ -ggdb -O0 -std=c++17 -I. -o app main.cpp console.cpp shell.cpp

