#pragma once

#ifdef USE_AIP33628_TIMEZONE_SELECT
#include "aip33628.h"
#include "esphome/components/select/select.h"
#include "esphome/core/preferences.h"

namespace esphome::aip33628 {

class TimezoneSelect : public select::Select, public Component {
 public:
  void set_panel(Aip33628Panel *panel) { panel_ = panel; }
  void add_timezone(const time::ParsedTimezone &tz) { zones_.push_back(tz); }
  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override {
    pref_ = global_preferences->make_preference<uint32_t>(this->get_object_id_hash());
    uint32_t saved = 0;
    size_t selected = 0;
    if (pref_.load(&saved)) {
      // Save the name's hash, so reordering the configured options is safe.
      for (size_t i = 1; i < this->size(); i++) {
        if (fnv1_hash(this->option_at(i)) == saved) {
          selected = i;
          break;
        }
      }
    }
    apply_(selected);
  }

 protected:
  void control(size_t index) override {
    apply_(index);
    uint32_t saved = index == 0 ? 0 : fnv1_hash(this->option_at(index));
    pref_.save(&saved);
  }
  void apply_(size_t index) {
    panel_->set_display_timezone(index == 0 ? nullptr : &zones_[index - 1]);
    this->publish_state(index);
  }

  Aip33628Panel *panel_{nullptr};
  std::vector<time::ParsedTimezone> zones_;
  ESPPreferenceObject pref_;
};

}  // namespace esphome::aip33628
#endif
