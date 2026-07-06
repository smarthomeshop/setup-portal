#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/wifi/wifi_component.h"

#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace smarthomeshop_setup_portal {

class SmartHomeShopSetupPortal : public Component, public AsyncWebHandler {
 public:
  void set_product_name(const std::string &product_name) { this->product_name_ = product_name; }
  void set_product_variant(const std::string &product_variant) { this->product_variant_ = product_variant; }
  void set_support_cloud(bool support_cloud) { this->support_cloud_ = support_cloud; }
  void set_support_home_assistant(bool support_home_assistant) { this->support_home_assistant_ = support_home_assistant; }
  void set_support_mqtt(bool support_mqtt) { this->support_mqtt_ = support_mqtt; }
  void set_support_homey(bool support_homey) { this->support_homey_ = support_homey; }
  void set_support_ethernet(bool support_ethernet) { this->support_ethernet_ = support_ethernet; }
  void set_support_firmware_selector(bool support_firmware_selector) { this->support_firmware_selector_ = support_firmware_selector; }
  void set_default_cloud_enabled(bool enabled) { this->default_cloud_enabled_ = enabled; }
  void set_default_home_assistant_enabled(bool enabled) { this->default_home_assistant_enabled_ = enabled; }
  void set_default_mqtt_enabled(bool enabled) { this->default_mqtt_enabled_ = enabled; }
  void set_firmware_options(const std::string &firmware_options) { this->firmware_options_raw_ = firmware_options; }
  void set_default_firmware_option(const std::string &firmware_option) { this->default_firmware_option_ = firmware_option; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

  bool should_apply_wifi() const { return this->pending_wifi_ && !this->wifi_apply_active_; }
  void begin_wifi_apply();
  std::string pending_wifi_ssid() const { return this->pending_wifi_ssid_; }
  std::string pending_wifi_password() const { return this->pending_wifi_password_; }
  void mark_wifi_connected();
  void mark_wifi_failed();

  bool cloud_enabled() const { return this->support_cloud_ && this->settings_.cloud_enabled != 0; }
  // Home Assistant (native ESPHome API) and Homey (local network) are always
  // available when supported by the product; they are not user-toggleable.
  bool home_assistant_enabled() const { return this->support_home_assistant_; }
  bool homey_enabled() const { return this->support_homey_; }
  bool mqtt_enabled() const { return this->support_mqtt_ && this->settings_.mqtt_enabled != 0; }
  std::string firmware_option() const { return this->settings_.firmware_option; }
  // True only after the user saved a firmware choice in the portal that has not
  // been applied to the product firmware selector yet.
  bool firmware_option_dirty() const { return this->support_firmware_selector_ && this->settings_.firmware_dirty != 0; }
  void mark_firmware_option_applied();

 protected:
  struct Settings {
    uint32_t magic{0};
    uint8_t cloud_enabled{1};
    uint8_t home_assistant_enabled{1};
    uint8_t mqtt_enabled{0};
    uint8_t firmware_dirty{0};
    char mqtt_host[64]{};
    char mqtt_port[8]{};
    char mqtt_username[48]{};
    char mqtt_password[64]{};
    char mqtt_topic[64]{};
    char firmware_option[96]{};
  };

  static constexpr uint32_t SETTINGS_MAGIC = 0x53485350;  // SHSP
  static constexpr uint32_t SETTINGS_PREF_KEY = 0x5A47A61D;
  static constexpr uint32_t SCAN_REFRESH_MS = 15000UL;
  static constexpr uint32_t WIFI_APPLY_TIMEOUT_MS = 45000UL;

  void load_settings_();
  void save_settings_();
  void apply_defaults_();

  void process_wifi_apply_(uint32_t now);
  bool current_wifi_matches_(const std::string &ssid) const;
  void restore_previous_wifi_();
  bool portal_should_handle_root_() const;
  bool is_known_captive_probe_(const std::string &url) const;
  void send_page_(AsyncWebServerRequest *request, const std::string &notice = "");
  void send_saved_page_(AsyncWebServerRequest *request);
  void send_status_(AsyncWebServerRequest *request);
  void save_from_request_(AsyncWebServerRequest *request);
  void send_response_(AsyncWebServerRequest *request, int code, const char *content_type, const std::string &content) const;
  void redirect_(AsyncWebServerRequest *request, const std::string &location) const;

  std::string render_network_options_() const;
  std::string render_firmware_options_() const;
  std::string render_checked_(bool value) const;
  std::string render_disabled_(bool value) const;
  std::string status_text_() const;
  std::string local_ip_() const;
  std::vector<std::string> firmware_options_() const;
  static std::string html_escape_(const std::string &value);
  static std::string json_escape_(const std::string &value);
  static std::string wifi_ap_ssid_(const wifi::WiFiAP &ap);
  static std::string wifi_ap_password_(const wifi::WiFiAP &ap);
  static std::string bounded_arg_(AsyncWebServerRequest *request, const char *name, size_t max_length);
  static void copy_to_(char *dest, size_t len, const std::string &value);

  ESPPreferenceObject pref_{};
  Settings settings_{};

  std::string product_name_{"SmartHomeShop device"};
  std::string product_variant_{"ESPHome firmware"};
  bool support_cloud_{true};
  bool support_home_assistant_{true};
  bool support_mqtt_{false};
  bool support_homey_{false};
  bool support_ethernet_{false};
  bool support_firmware_selector_{false};
  bool default_cloud_enabled_{true};
  bool default_home_assistant_enabled_{true};
  bool default_mqtt_enabled_{false};
  std::string firmware_options_raw_{};
  std::string default_firmware_option_{};

  bool pending_wifi_{false};
  bool wifi_apply_active_{false};
  bool wifi_last_error_{false};
  bool previous_sta_valid_{false};
  uint32_t wifi_apply_started_ms_{0};
  uint32_t last_scan_request_ms_{0};
  wifi::WiFiAP previous_sta_{};
  std::string pending_wifi_ssid_{};
  std::string pending_wifi_password_{};
  std::string last_notice_{};
};

}  // namespace smarthomeshop_setup_portal
}  // namespace esphome
