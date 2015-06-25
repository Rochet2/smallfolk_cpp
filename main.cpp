// main.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Header.h"

int main()
{
    // some test data
    TT ttt(TTABLE);
    ttt.set(6.0, 1255);
    ttt.set(6.0, 1255);
    ttt.set(false, 123.0);
    ttt.set("asd", "bsd");
    ttt.set("TEST", true);
    ttt.set("TEST2", ttt.tbl());
    ttt.set("TEST3", ttt.tbl());
    ttt.set("TEST2", ttt.tbl());
    ttt.set("TEST9001", ttt.get(false));

    std::string dumped = ttt.dumps(); // dump
    TT loaded = loads(dumped); // load

    std::cout << (dumped) << std::endl << std::endl;
    std::cout << (loaded.dumps()) << std::endl; // dump again
    return 0;
}
