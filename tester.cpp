#include "libs/ytone/tonegen.h"
#include "libs/ytone/tonedetect.h"

#include <iostream>

int main() {
  std::cout << "Hello world!" << std::endl;
  String name = "cotv";
  String prefix = "";

  ToneConsumer* detect = new ToneConsumer("","tone/cotv");

  ToneSource* source = ToneSource::getTone(name,prefix);
  source->attach(detect);
  std::cout << "before run" << std::endl;
  source->start();
  std::cout << "after run" << std::endl;

  while(source->running()) {
    // std::cout << "running" << std::endl;
  }
  // source->stop();
  return 0;
}
