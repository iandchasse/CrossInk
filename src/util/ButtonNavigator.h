#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>

#include "MappedInputManager.h"

class ButtonNavigator final {
  using Callback = std::function<void()>;
  using Button = MappedInputManager::Button;
  // A ready-made pair of keys: getNextButtons()/getPreviousButtons() and callers that build a std::array
  // themselves (Settings' std::array{up, up}).
  using Buttons = std::array<Button, 2>;
  // A braced list written at the call site. It must not bind to Buttons: {Button::Left} would then leave the
  // second slot value-initialised to Button 0, which is Back, and a Back press would fire the handler too.
  // Overload resolution prefers std::initializer_list for a braced list of any length, so a one-key list
  // stays one key. The list is borrowed, consumed synchronously and never retained.
  using ButtonList = std::initializer_list<Button>;

  const uint16_t continuousStartMs;
  const uint16_t continuousIntervalMs;
  uint32_t lastContinuousNavTime = 0;
  static const MappedInputManager* mappedInput;

  [[nodiscard]] bool shouldNavigateContinuously() const;
  void onPressImpl(const Button* buttons, size_t count, const Callback& callback);
  void onReleaseImpl(const Button* buttons, size_t count, const Callback& callback);
  void onContinuousImpl(const Button* buttons, size_t count, const Callback& callback);

 public:
  explicit ButtonNavigator(const uint16_t continuousIntervalMs = 500, const uint16_t continuousStartMs = 500)
      : continuousStartMs(continuousStartMs), continuousIntervalMs(continuousIntervalMs) {}

  static void setMappedInputManager(const MappedInputManager& mappedInputManager) { mappedInput = &mappedInputManager; }

  void onNext(const Callback& callback);
  void onPrevious(const Callback& callback);
  void onPressAndContinuous(const Buttons& buttons, const Callback& callback);
  void onPressAndContinuous(ButtonList buttons, const Callback& callback);

  void onNextPress(const Callback& callback);
  void onPreviousPress(const Callback& callback);
  void onPress(const Buttons& buttons, const Callback& callback);
  void onPress(ButtonList buttons, const Callback& callback);

  void onNextRelease(const Callback& callback);
  void onPreviousRelease(const Callback& callback);
  void onRelease(const Buttons& buttons, const Callback& callback);
  void onRelease(ButtonList buttons, const Callback& callback);

  void onNextContinuous(const Callback& callback);
  void onPreviousContinuous(const Callback& callback);
  void onContinuous(const Buttons& buttons, const Callback& callback);
  void onContinuous(ButtonList buttons, const Callback& callback);

  [[nodiscard]] static int nextIndex(int currentIndex, int totalItems);
  [[nodiscard]] static int previousIndex(int currentIndex, int totalItems);

  [[nodiscard]] static int nextPageIndex(int currentIndex, int totalItems, int itemsPerPage);
  [[nodiscard]] static int previousPageIndex(int currentIndex, int totalItems, int itemsPerPage);

  [[nodiscard]] static constexpr Buttons getNextButtons() {
    return {MappedInputManager::Button::Down, MappedInputManager::Button::Right};
  }
  [[nodiscard]] static constexpr Buttons getPreviousButtons() {
    return {MappedInputManager::Button::Up, MappedInputManager::Button::Left};
  }
};
