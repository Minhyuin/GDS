#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void initGDS();
void initGDSBsp();
void initGDSApp();

#ifdef __cplusplus
}

#include "Singleton.hpp"

class GDS : public Singleton<GDS> {
public:
  void init();
  void initBsp();
  void initApp();

private:
  GDS() = default;
  friend class Singleton<GDS>;

  void bspInit();

  void coreInit();
};

#endif
