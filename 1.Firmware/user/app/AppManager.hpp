#pragma once

#include "Lazy.hpp"
#include "Singleton.hpp"
#include "Buzzer.hpp"
#include "WGS.hpp"

class AppManager : public Singleton<AppManager> {
public:
  void initApp();

private:
  AppManager() {};
  friend class Singleton<AppManager>;

  void createApp();

  Lazy<APP::BUZZER::BuzzerTask> buzzer_;
  Lazy<APP::WGS::WGSManager> wgs_;
};
