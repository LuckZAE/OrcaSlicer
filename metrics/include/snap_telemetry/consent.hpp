#pragma once
namespace snap {
struct IConsentProvider { virtual ~IConsentProvider()=default;
  virtual bool is_allowed() = 0; };
}
