#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void initGDS();

#ifdef __cplusplus
}

#include "Singleton.hpp"

class GDS : public Singleton<GDS> {
public:
  void init();

private:
  GDS() = default;
  friend class Singleton<GDS>;

  void bspInit();

  void coreInit();
};

#endif
