#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <stdlib.h>
#include <sys/stat.h>

#include "debug.h"
#include "types.h"
#include "core.h"
#include "args.h"

using namespace vortex;

int main(int argc, char **argv) {

  std::string archString("rv32i");
  int num_cores(1);
  int num_warps(NUM_WARPS);
  int num_threads(NUM_THREADS);
  std::string imgFileName;
  bool showHelp(false);
  bool showStats(false);

  /* Read the command line arguments. */
  CommandLineArgFlag fh("-h", "--help", "", showHelp);
  CommandLineArgSetter<std::string> fa("-a", "--arch", "", archString);  
  CommandLineArgSetter<std::string> fi("-i", "--image", "", imgFileName);
  CommandLineArgSetter<int> fc("-c", "--cores", "", num_cores);
  CommandLineArgSetter<int> fw("-w", "--warps", "", num_warps);
  CommandLineArgSetter<int> ft("-t", "--threads", "", num_threads);
  CommandLineArgFlag fs("-s", "--stats", "", showStats);

  CommandLineArg::readArgs(argc - 1, argv + 1);

  if (showHelp || imgFileName.empty()) {
    std::cout << "Vortex emulator command line arguments:\n"
                 "  -i, --image <filename>   Program RAM image\n"
                 "  -c, --cores <num> Number of cores\n"
                 "  -w, --warps <num> Number of warps\n"
                 "  -t, --threads <num> Number of threads\n"
                 "  -a, --arch <arch string> Architecture string\n"
                 "  -s, --stats              Print stats on exit.\n";
    return 0;
  }

  ArchDef arch(archString, num_cores, num_warps, num_threads);

  Decoder decoder(arch);
  MemoryUnit mu(4096, arch.getWordSize(), true);
  
  RAM old_ram;
  old_ram.loadHexImpl(imgFileName.c_str());
  mu.attach(old_ram, 0);

  struct stat hello;
  fstat(0, &hello);

  std::vector<std::shared_ptr<Core>> cores(num_cores);
  for (int i = 0; i < num_cores; ++i) {
    cores[i] = std::make_shared<Core>(arch, decoder, mu);
  }

  bool running;

  do {
    running = false;
    for (int i = 0; i < num_cores; ++i) {
      if (!cores[i]->running())
        continue;
      running = true;
      cores[i]->step();
    }
  } while (running);

  return 0;
}
