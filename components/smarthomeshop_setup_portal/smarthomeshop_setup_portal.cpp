#include "smarthomeshop_setup_portal.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>

namespace esphome {
namespace smarthomeshop_setup_portal {

static const char *const TAG = "smarthomeshop_setup_portal";

// Officieel SmartHomeShop merk-icoon (zelfde SVG als smarthomeshopioapp), wit op brand-gradient.
static const char *const BRAND_MARK_SVG =
    "<svg viewBox=\"0 0 772.9 607.6\" fill=\"#fff\" xmlns=\"http://www.w3.org/2000/svg\">"
    "<path d=\"M636.8,285.9c-0.5-10.6-11-15.9-18.8-21.6c-40.6-30.1-81.2-60.1-121.8-90.2c-34.5-25.6-69-51.1-103.5-76.7 c-3.2-2.4-9.4-2.4-12.6,0c-71.6,53.1-143.3,106.1-214.9,159.2c-5.8,4.3-11.6,8.6-17.4,12.9c-3.2,2.4-8.1,5.2-10.1,8.6 c-4.8,8.3-1.7,24.7-1.7,33.6c0,52.9,0,105.8,0,158.7c0,41.6,0,83.1,0,124.7c0,6.7,5.7,12.5,12.5,12.5c30.9,0,61.9,0,92.8,0 c16.1,0,16.1-25,0-25c-26.8,0-53.6,0-80.4,0c0-86.7,0-173.3,0-260c0-10.7,0-21.4,0-32c67.3-49.9,134.6-99.7,202-149.6 c7.8-5.8,15.6-11.6,23.5-17.4c67.3,49.8,134.6,99.7,201.8,149.5c7.9,5.8,15.7,11.6,23.6,17.5c0,88.8,0,177.5,0,266.3 c0,8.6,0,17.2,0,25.8c-26.8,0-53.6,0-80.4,0c-16.1,0-16.1,25,0,25c30.9,0,61.9,0,92.8,0c6.7,0,12.5-5.7,12.5-12.5 c0-89.3,0-178.7,0-268C636.8,313.4,637.4,299.6,636.8,285.9z\"/>"
    "<path d=\"M261.7,428.8c0,27.7,13.7,53.7,36,69.9c17.3,12.5,37.1,16,58,16c18.5,0,37,0,55.4,0c19.1,0,37.3-0.9,54.7-10.2 c27.6-14.6,45.4-44.5,45.4-75.7c0-16.1-25-16.1-25,0c0,29.1-21.2,54.6-49.8,60c-16,3-33.9,1-50,1c-16.1,0-34,2-50-1 c-28.6-5.4-49.8-30.9-49.8-60C286.6,412.8,261.7,412.7,261.7,428.8L261.7,428.8z\"/>"
    "<circle cx=\"310.9\" cy=\"351.6\" r=\"21.4\"/>"
    "<circle cx=\"462\" cy=\"351.6\" r=\"21.4\"/>"
    "<path d=\"M767.5,279.4c-42.2-31.3-84.5-62.6-126.7-93.8C573.5,135.7,506.3,85.9,439,36c-15.4-11.4-30.8-22.8-46.2-34.3 c-3.2-2.4-9.4-2.4-12.6,0c-42.2,31.3-84.5,62.6-126.7,93.8c-67.3,49.8-134.6,99.7-201.9,149.5C36.2,256.5,20.8,268,5.4,279.4 c-12.8,9.5-0.3,31.1,12.6,21.5c42.2-31.3,84.5-62.6,126.7-93.8c67.3-49.8,134.6-99.7,201.9-149.5c13.3-9.9,26.6-19.7,40-29.6 c40.1,29.7,80.3,59.4,120.4,89.2c67.3,49.8,134.6,99.7,201.9,149.5c15.4,11.4,30.8,22.8,46.2,34.3 C767.8,310.5,780.3,288.8,767.5,279.4z\"/>"
    "</svg>";

void SmartHomeShopSetupPortal::setup() {
  this->apply_defaults_();
  this->load_settings_();

  if (wifi::global_wifi_component != nullptr) {
    wifi::global_wifi_component->set_keep_scan_results(true);
  }

  if (web_server_base::global_web_server_base == nullptr) {
    ESP_LOGW(TAG, "web_server_base is not available; setup portal disabled");
    return;
  }

  web_server_base::global_web_server_base->add_handler_without_auth(this);
  ESP_LOGI(TAG, "SmartHomeShop setup portal ready");
}

void SmartHomeShopSetupPortal::loop() {
  if (wifi::global_wifi_component == nullptr)
    return;

  const uint32_t now = millis();
  if (this->pending_wifi_ && !this->wifi_apply_active_)
    this->begin_wifi_apply();
  if (this->wifi_apply_active_)
    this->process_wifi_apply_(now);

  // Only refresh the network list while idle in the portal. Never scan while a
  // connection attempt is in progress: scanning and connecting share the radio,
  // so periodic scans repeatedly drop the STA connection and it never comes up.
  const bool connecting = this->pending_wifi_ || this->wifi_apply_active_ || this->wifi_provisioning_started_;
  if (!connecting && this->portal_should_handle_root_() &&
      (this->last_scan_request_ms_ == 0 || now - this->last_scan_request_ms_ > SCAN_REFRESH_MS)) {
    this->last_scan_request_ms_ = now == 0 ? 1 : now;
    wifi::global_wifi_component->set_keep_scan_results(true);
    wifi::global_wifi_component->start_scanning();
  }
}

void SmartHomeShopSetupPortal::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SmartHomeShop Setup Portal:\n"
                "  Product: %s\n"
                "  Variant: %s\n"
                "  Cloud option: %s\n"
                "  Home Assistant option: %s\n"
                "  MQTT option: %s\n"
                "  Homey hint: %s\n"
                "  Ethernet hint: %s\n"
                "  Firmware selector: %s",
                this->product_name_.c_str(), this->product_variant_.c_str(), YESNO(this->support_cloud_),
                YESNO(this->support_home_assistant_), YESNO(this->support_mqtt_), YESNO(this->support_homey_),
                YESNO(this->support_ethernet_), YESNO(this->support_firmware_selector_));
}

bool SmartHomeShopSetupPortal::canHandle(AsyncWebServerRequest *request) const {
  if (request == nullptr)
    return false;

  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const std::string url(request->url_to(url_buf));

  if (request->method() == HTTP_GET) {
    if (url == "/setup" || url == "/setup/status" || url == "/favicon.ico")
      return true;
    if (url == "/" || this->is_known_captive_probe_(url))
      return this->portal_should_handle_root_();
  }

  return request->method() == HTTP_POST && url == "/setup/save";
}

void SmartHomeShopSetupPortal::handleRequest(AsyncWebServerRequest *request) {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const std::string url(request->url_to(url_buf));

  if (url == "/favicon.ico") {
    this->send_response_(request, 204, "image/x-icon", "");
    return;
  }

  if (request->method() == HTTP_POST && url == "/setup/save") {
    if (this->save_from_request_(request)) {
      this->send_saved_page_(request);
    } else {
      // Validation failed (e.g. no Wi-Fi network chosen); re-show the form.
      this->send_page_(request, this->last_notice_);
    }
    return;
  }

  if (url == "/setup/status") {
    this->send_status_(request);
    return;
  }

  // Captive-portal probe URLs and the fallback root both get the branded setup
  // page served inline with a 200. Serving the page directly (rather than a 302
  // redirect) is the most reliable cross-OS trigger for the automatic sign-in
  // popup, matching how ESPHome's own captive_portal responds.
  this->send_page_(request, this->last_notice_);
}

void SmartHomeShopSetupPortal::begin_wifi_apply() {
  if (!this->pending_wifi_ || this->wifi_apply_active_)
    return;

  auto *wifi = wifi::global_wifi_component;
  if (wifi == nullptr) {
    this->mark_wifi_failed();
    return;
  }

  // Hand the credentials to ESPHome's native Wi-Fi provisioning, the same path
  // the stock captive_portal uses. It manages the AP teardown, the shared
  // AP/STA channel and the connection retries. Driving set_sta/start_connecting
  // by hand fought the Wi-Fi state machine (repeated disable/enable mid-connect)
  // and the STA never came up.
  wifi->save_wifi_sta(this->pending_wifi_ssid_, this->pending_wifi_password_);

  this->wifi_provisioning_started_ = true;
  this->wifi_apply_active_ = true;
  this->wifi_last_error_ = false;
  this->wifi_apply_started_ms_ = millis() == 0 ? 1 : millis();
  this->last_notice_ = "Connecting to your Wi-Fi network.";
}

void SmartHomeShopSetupPortal::mark_wifi_connected() {
  this->pending_wifi_ = false;
  this->wifi_apply_active_ = false;
  this->wifi_last_error_ = false;
  this->previous_sta_valid_ = false;
  this->wifi_apply_started_ms_ = 0;
  this->last_notice_ = "Wi-Fi connected.";
}

void SmartHomeShopSetupPortal::mark_wifi_failed() {
  this->pending_wifi_ = false;
  this->wifi_apply_active_ = false;
  this->wifi_last_error_ = true;
  this->previous_sta_valid_ = false;
  this->wifi_apply_started_ms_ = 0;
  this->last_notice_ = "Couldn't connect to Wi-Fi. Check the network name and password, then try again.";
}

void SmartHomeShopSetupPortal::process_wifi_apply_(uint32_t now) {
  if (this->current_wifi_matches_(this->pending_wifi_ssid_)) {
    this->mark_wifi_connected();
    return;
  }

  if (this->wifi_apply_started_ms_ != 0 && now - this->wifi_apply_started_ms_ > WIFI_APPLY_TIMEOUT_MS)
    this->mark_wifi_failed();
}

bool SmartHomeShopSetupPortal::current_wifi_matches_(const std::string &ssid) const {
  if (ssid.empty() || wifi::global_wifi_component == nullptr || !wifi::global_wifi_component->is_connected())
    return false;

  char ssid_buf[wifi::SSID_BUFFER_SIZE]{};
  wifi::global_wifi_component->wifi_ssid_to(ssid_buf);
  return ssid == ssid_buf;
}

void SmartHomeShopSetupPortal::restore_previous_wifi_() {
  auto *wifi = wifi::global_wifi_component;
  if (wifi == nullptr)
    return;

  wifi->disable();
  if (this->previous_sta_valid_) {
    wifi->save_wifi_sta(this->wifi_ap_ssid_(this->previous_sta_), this->wifi_ap_password_(this->previous_sta_));
  } else {
    wifi->clear_sta();
  }
  wifi->enable();
}

void SmartHomeShopSetupPortal::apply_defaults_() {
  this->settings_.magic = SETTINGS_MAGIC;
  this->settings_.cloud_enabled = this->default_cloud_enabled_ ? 1 : 0;
  this->settings_.home_assistant_enabled = this->default_home_assistant_enabled_ ? 1 : 0;
  this->settings_.mqtt_enabled = this->default_mqtt_enabled_ ? 1 : 0;
  this->settings_.firmware_dirty = 0;
  this->copy_to_(this->settings_.mqtt_port, sizeof(this->settings_.mqtt_port), "1883");
  this->copy_to_(this->settings_.mqtt_topic, sizeof(this->settings_.mqtt_topic), "smarthomeshop/" + App.get_name());
  const std::string firmware_option =
      !this->default_firmware_option_.empty() ? this->default_firmware_option_ : this->product_variant_;
  this->copy_to_(this->settings_.firmware_option, sizeof(this->settings_.firmware_option), firmware_option);
}

void SmartHomeShopSetupPortal::load_settings_() {
  if (global_preferences == nullptr)
    return;

  this->pref_ = global_preferences->make_preference<Settings>(SETTINGS_PREF_KEY, true);
  Settings loaded{};
  if (this->pref_.load(&loaded) && loaded.magic == SETTINGS_MAGIC) {
    this->settings_ = loaded;
  } else {
    this->save_settings_();
  }
}

void SmartHomeShopSetupPortal::save_settings_() {
  this->settings_.magic = SETTINGS_MAGIC;
  if (this->pref_.save(&this->settings_) && global_preferences != nullptr)
    global_preferences->sync();
}

bool SmartHomeShopSetupPortal::portal_should_handle_root_() const {
  if (wifi::global_wifi_component == nullptr)
    return true;
  return wifi::global_wifi_component->is_ap_active() || !wifi::global_wifi_component->is_connected();
}

bool SmartHomeShopSetupPortal::is_known_captive_probe_(const std::string &url) const {
  // OS connectivity-check endpoints. Answering these with our page (200) is
  // what triggers the automatic captive-portal popup.
  return url == "/generate_204" || url == "/gen_204" || url == "/generate204" ||     // Android
         url == "/hotspot-detect.html" || url == "/hotspotdetect.html" ||            // Apple iOS/macOS
         url == "/library/test/success.html" || url == "/success.html" ||           // Apple / misc
         url == "/connecttest.txt" || url == "/ncsi.txt" || url == "/fwlink" ||      // Windows
         url == "/redirect" || url == "/canonical.html" || url == "/success.txt" ||  // Windows / Firefox
         url == "/check_network_status.txt" || url == "/mobile/status.php";          // misc Android/other
}

void SmartHomeShopSetupPortal::send_response_(AsyncWebServerRequest *request, int code, const char *content_type,
                                               const std::string &content) const {
  auto *response = request->beginResponse(code, content_type, content);
  response->addHeader("Cache-Control", "no-store, max-age=0");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Connection", "close");
  request->send(response);
}

void SmartHomeShopSetupPortal::redirect_(AsyncWebServerRequest *request, const std::string &location) const {
  auto *response = request->beginResponse(302, "text/plain; charset=utf-8", "Redirecting");
  response->addHeader("Location", location.c_str());
  response->addHeader("Cache-Control", "no-store, max-age=0");
  response->addHeader("Connection", "close");
  request->send(response);
}

std::string SmartHomeShopSetupPortal::render_checked_(bool value) const { return value ? " checked" : ""; }
std::string SmartHomeShopSetupPortal::render_disabled_(bool value) const { return value ? "" : " disabled"; }

void SmartHomeShopSetupPortal::mark_firmware_option_applied() {
  if (this->settings_.firmware_dirty == 0)
    return;
  this->settings_.firmware_dirty = 0;
  this->save_settings_();
}

std::string SmartHomeShopSetupPortal::local_ip_() const {
  if (wifi::global_wifi_component == nullptr || !wifi::global_wifi_component->is_connected())
    return {};
  for (const auto &ip : wifi::global_wifi_component->get_ip_addresses()) {
    char buf[network::IP_ADDRESS_BUFFER_SIZE];
    ip.str_to(buf);
    std::string value(buf);
    if (value.empty() || value == "0.0.0.0" || value.find(':') != std::string::npos)
      continue;
    return value;
  }
  return {};
}

std::string SmartHomeShopSetupPortal::status_text_() const {
  if (this->wifi_apply_active_)
    return "Connecting Wi-Fi";
  if (this->wifi_last_error_)
    return "Wi-Fi error";
  if (wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected())
    return "Online";
  if (wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_ap_active())
    return "Setup mode";
  return "Offline";
}

std::string SmartHomeShopSetupPortal::render_network_options_() const {
  std::string html;
  html.reserve(1200);
  html += "<option value=''>Choose a network or type it manually</option>";

  if (wifi::global_wifi_component == nullptr)
    return html;

  std::set<std::string> seen;
  for (const auto &scan : wifi::global_wifi_component->get_scan_result()) {
    if (scan.get_is_hidden())
      continue;
    std::string ssid(scan.get_ssid().c_str(), scan.get_ssid().size());
    if (ssid.empty() || seen.count(ssid) != 0)
      continue;
    seen.insert(ssid);
    html += "<option value='";
    html += html_escape_(ssid);
    html += "'>";
    html += html_escape_(ssid);
    html += " (";
    html += std::to_string(scan.get_rssi());
    html += " dBm)</option>";
  }

  return html;
}

std::vector<std::string> SmartHomeShopSetupPortal::firmware_options_() const {
  std::vector<std::string> options;
  if (!this->support_firmware_selector_)
    return options;

  auto trim = [](std::string value) -> std::string {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
      return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
  };

  std::string item;
  for (char c : this->firmware_options_raw_) {
    if (c == '|') {
      item = trim(item);
      if (!item.empty())
        options.push_back(item);
      item.clear();
    } else {
      item.push_back(c);
    }
  }
  item = trim(item);
  if (!item.empty())
    options.push_back(item);

  const std::string fallback = !this->default_firmware_option_.empty() ? this->default_firmware_option_ : this->product_variant_;
  if (options.empty() && !fallback.empty())
    options.push_back(fallback);

  return options;
}

std::string SmartHomeShopSetupPortal::render_firmware_options_() const {
  std::string html;
  const std::string selected = this->settings_.firmware_option[0] != '\0' ? this->settings_.firmware_option : this->product_variant_;
  bool selected_found = false;
  for (const auto &option : this->firmware_options_()) {
    html += "<option value='";
    html += html_escape_(option);
    html += "'";
    if (option == selected) {
      html += " selected";
      selected_found = true;
    }
    html += ">";
    html += html_escape_(option);
    html += "</option>";
  }
  if (!selected.empty() && !selected_found) {
    html += "<option value='";
    html += html_escape_(selected);
    html += "' selected>";
    html += html_escape_(selected);
    html += "</option>";
  }
  return html;
}

void SmartHomeShopSetupPortal::send_page_(AsyncWebServerRequest *request, const std::string &notice) {
  std::string html;
  html.reserve(28000);
  const bool connected = wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected();
  char current_ssid[wifi::SSID_BUFFER_SIZE]{};
  if (connected)
    wifi::global_wifi_component->wifi_ssid_to(current_ssid);

  const std::string selected_firmware =
      this->settings_.firmware_option[0] != '\0' ? this->settings_.firmware_option : this->product_variant_;
  const int visible_integrations = (this->support_cloud_ ? 1 : 0) + (this->support_home_assistant_ ? 1 : 0) +
                                   (this->support_mqtt_ ? 1 : 0) + (this->support_homey_ ? 1 : 0);
  const bool selected_ethernet = selected_firmware.find("Ethernet") != std::string::npos;
  const auto firmware_options = this->firmware_options_();

  std::string firmware_options_json = "[";
  for (size_t i = 0; i < firmware_options.size(); i++) {
    if (i != 0)
      firmware_options_json += ",";
    firmware_options_json += "\"";
    firmware_options_json += json_escape_(firmware_options[i]);
    firmware_options_json += "\"";
  }
  firmware_options_json += "]";

  html += R"SHHTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><title>SmartHomeShop Setup</title><style>
:root{--ink:#0a0a0a;--muted:#6b7280;--bg:#f7f9fb;--card:#ffffff;--line:#e5e7eb;--line-strong:#d1d5db;--blue:#2563eb;--blue-dark:#1d4ed8;--teal:#20c8b0;--brand:linear-gradient(135deg,#338cf5,#4fd1c5);--shadow:0 1px 2px rgba(16,24,40,.05),0 1px 3px rgba(16,24,40,.06)}*{box-sizing:border-box}body{margin:0;min-height:100svh;padding:env(safe-area-inset-top) 0 calc(28px + env(safe-area-inset-bottom));font-family:ui-sans-serif,system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;color:var(--ink);background:linear-gradient(180deg,#ffffff 0%,var(--bg) 34%,var(--bg) 100%);-webkit-font-smoothing:antialiased}a{color:var(--blue-dark)}.shell{width:min(760px,100%);margin:0 auto;padding:16px}.hero,.panel{border:1px solid var(--line);border-radius:20px;background:var(--card);box-shadow:var(--shadow)}.hero{position:relative;overflow:hidden}.hero:after{content:"";position:absolute;inset:0 0 auto;height:4px;background:linear-gradient(90deg,#338cf5,#4fd1c5)}.hero-inner{position:relative;padding:22px}.brand{display:flex;align-items:center;gap:12px}.mark{width:42px;height:42px;border-radius:12px;background:var(--brand);display:grid;place-items:center;flex:0 0 auto}.mark svg{width:24px;height:19px;display:block}.eyebrow{font-size:11px;text-transform:uppercase;letter-spacing:.14em;color:var(--blue);font-weight:700}.brand strong{display:block;font-size:16px;font-weight:600;letter-spacing:-.01em}.hero h1{max-width:560px;margin:18px 0 8px;font-size:clamp(24px,6.4vw,34px);line-height:1.12;letter-spacing:-.03em;font-weight:700}.hero p{max-width:560px;margin:0;color:var(--muted);font-size:15px;line-height:1.55}.topline{display:flex;gap:8px;flex-wrap:wrap;margin-top:16px}.chip{display:inline-flex;align-items:center;gap:7px;border:1px solid var(--line);border-radius:999px;background:#f9fafb;padding:7px 12px;font-size:12.5px;font-weight:600;color:#374151}.chip.dark{border-color:#1e293b;background:#0f172a;color:#f1f5f9}.dot{width:8px;height:8px;border-radius:50%;background:#38bdf8;box-shadow:0 0 0 4px rgba(56,189,248,.16)}.dot.online{background:#10b981;box-shadow:0 0 0 4px rgba(16,185,129,.16)}.map{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:12px 0}.map div{border:1px solid var(--line);border-radius:14px;background:var(--card);padding:12px 14px;font-weight:600;font-size:14px;box-shadow:var(--shadow)}.map b{display:inline-grid;place-items:center;width:22px;height:22px;margin-right:7px;border-radius:50%;background:var(--brand);color:#fff;font-size:12px;font-weight:700}.map span{display:block;margin-top:4px;color:var(--muted);font-size:12.5px;font-weight:400}.panel{padding:20px;margin-top:12px}.section-title{margin:0 0 14px}.section-title h2{margin:0;font-size:19px;letter-spacing:-.02em;font-weight:700}.section-title p{margin:4px 0 0;color:var(--muted);font-size:13.5px}.notice{border:1px solid #a7f3d0;background:#ecfdf5;color:#065f46;border-radius:12px;padding:12px 14px;margin-bottom:14px;font-weight:500;font-size:14px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}.field{margin:0 0 12px}.field label{display:block;font-weight:600;font-size:14px;margin-bottom:6px}.label-row{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-bottom:6px}.label-row label{margin:0}.mini-btn{appearance:none;border:1px solid var(--line);background:#fff;border-radius:999px;padding:4px 10px;font:inherit;font-size:12px;font-weight:600;color:var(--blue-dark);cursor:pointer}.hint{margin-top:6px;color:var(--muted);font-size:12.5px;line-height:1.45}.eth-hint{display:none;margin:-6px 0 12px}.eth-hint.show{display:block}.wifi-err{display:none;margin-top:6px;color:#dc2626;font-size:12.5px;font-weight:600;line-height:1.4}.wifi-err.show{display:block}input.invalid{border-color:#dc2626;box-shadow:0 0 0 3px rgba(220,38,38,.15)}.input,input,select{width:100%;min-height:48px;border:1px solid var(--line-strong);border-radius:12px;background:#fff;color:var(--ink);padding:12px 14px;font:inherit;font-size:16px;outline:none;transition:border-color .15s,box-shadow .15s}input:focus,select:focus{border-color:var(--blue);box-shadow:0 0 0 3px rgba(37,99,235,.15)}.pw-wrap{position:relative}.pw-wrap input{padding-right:78px}.pw-toggle{position:absolute;right:6px;top:50%;transform:translateY(-50%);border:0;background:#f3f4f6;border-radius:8px;padding:6px 10px;font:inherit;font-size:12.5px;font-weight:600;color:#374151;cursor:pointer}.choice-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:2px 0 12px}.choice-card{position:relative;display:block;border:1px solid var(--line);border-radius:14px;background:#fff;padding:14px;cursor:pointer;transition:border-color .15s,background .15s}.choice-card input{position:absolute;opacity:0;pointer-events:none}.choice-card.active{border-color:var(--blue);background:#eff6ff;box-shadow:inset 0 0 0 1px var(--blue)}.choice-card strong{display:block;font-size:15px;font-weight:600;letter-spacing:-.01em}.choice-card span{display:block;margin-top:4px;color:var(--muted);font-size:13px;line-height:1.45}.firmware-summary{border:1px solid #b9e6df;border-radius:14px;background:#f0fbf9;padding:13px 15px;margin:10px 0 0}.firmware-summary strong{display:block;font-size:12px;text-transform:uppercase;letter-spacing:.08em;color:#0f766e;font-weight:700}.firmware-summary span{display:block;margin-top:4px;color:#134e4a;font-size:14px;line-height:1.4;font-weight:600}.firmware-summary span+span{margin-top:3px;font-weight:400;color:#5f7470;font-size:12.5px}.integrations{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}.integrations.one{grid-template-columns:1fr}.integrations.two{grid-template-columns:repeat(2,1fr)}.checkcard{position:relative;display:flex;gap:11px;align-items:flex-start;padding:14px;border:1px solid var(--line);border-radius:14px;background:#fff;cursor:pointer;transition:border-color .15s,background .15s}.checkcard input{position:absolute;opacity:0;pointer-events:none}.tick{flex:0 0 auto;width:26px;height:26px;border-radius:8px;border:1px solid var(--line-strong);background:#f9fafb;display:grid;place-items:center;color:transparent;font-size:15px;font-weight:800;transition:background .15s,border-color .15s}.checkcard.on{border-color:var(--blue);background:#eff6ff;box-shadow:inset 0 0 0 1px var(--blue)}.checkcard.on .tick{background:var(--brand);border-color:transparent;color:#fff}.checkcard.locked{cursor:default;background:#f7f8f9;border-color:var(--line);box-shadow:none}.checkcard.locked .tick{background:#9aa4ad;border-color:transparent;color:#fff}.checkcard.locked strong{display:flex;align-items:center;gap:8px;flex-wrap:wrap;color:#4b5563}.checkcard.locked em{font-style:normal;font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.05em;color:#5b6b64;background:#eceff1;border-radius:999px;padding:2px 7px}.checkcard.locked span span{color:#7b8590}.checkcard strong{display:block;font-size:15px;font-weight:600;letter-spacing:-.01em}.checkcard span span{display:block;margin-top:3px;color:var(--muted);font-size:12.5px;line-height:1.45}.advanced{display:none;border:1px dashed var(--line-strong);border-radius:14px;background:#f9fafb;padding:14px;margin-top:12px}.advanced.show{display:block}.actions{display:grid;gap:12px;margin-top:18px}.btn{appearance:none;border:0;border-radius:12px;background:var(--blue);color:#fff;font-weight:600;padding:14px 22px;min-height:50px;font-size:15.5px;cursor:pointer;box-shadow:0 12px 26px -20px rgba(37,99,235,.65);transition:background .15s}.btn.block{width:100%}.btn:hover{background:var(--blue-dark)}.ghost{font-weight:600;font-size:14px;text-decoration:none}.micro{margin:14px 0 0;text-align:center;color:var(--muted);font-size:12.5px;line-height:1.5}@media(max-width:640px){.shell{padding:12px}.hero,.panel{border-radius:18px}.hero-inner,.panel{padding:16px}.grid,.choice-grid,.integrations,.integrations.two,.integrations.one{grid-template-columns:1fr}.map{gap:6px}.map div{padding:10px 8px;text-align:center;font-size:12.5px}.map b{display:grid;margin:0 auto 4px}.map span{display:none}.actions{display:grid}.btn{width:100%}.topline .chip{font-size:12px}}@media(prefers-reduced-motion:no-preference){.hero,.map div,.panel{animation:rise .32s ease-out both}.map div:nth-child(2){animation-delay:.04s}.map div:nth-child(3){animation-delay:.08s}.panel{animation-delay:.1s}@keyframes rise{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:none}}}
</style></head><body><main class="shell"><section class="hero"><div class="hero-inner"><div class="brand"><div class="mark">)SHHTML";
  html += BRAND_MARK_SVG;
  html += R"SHHTML(</div><div><div class="eyebrow">Device setup</div><strong>)SHHTML";
  html += html_escape_(this->product_name_);
  html += R"SHHTML(</strong></div></div><h1>Let&rsquo;s connect your device</h1><p>Connect it to your network, pick how it links up, and choose your integrations. We&rsquo;ll match the right firmware for you.</p><div class="topline"><span class="chip dark"><i class="dot )SHHTML";
  html += (connected ? "online" : "");
  html += R"SHHTML("></i>)SHHTML";
  html += html_escape_(this->status_text_());
  html += R"SHHTML(</span><span class="chip">)SHHTML";
  html += (connected ? std::string("Online via ") + html_escape_(current_ssid) : std::string("Fallback portal &middot; 192.168.4.1"));
  html += R"SHHTML(</span></div></div></section><section class="panel"><div class="section-title"><h2>Network</h2><p>Pick a network from the list, or type its name yourself.</p></div>)SHHTML";

  if (!notice.empty()) {
    html += "<div class='notice'>";
    html += html_escape_(notice);
    html += "</div>";
  }

  html += R"SHHTML(<form method="post" action="/setup/save" id="setup_form"><div class="grid"><div class="field"><div class="label-row"><label for="wifi_select">Wi-Fi network</label><button type="button" class="mini-btn" onclick="location.reload()">&#8635; Refresh</button></div><select id="wifi_select" name="wifi_ssid_select" onchange="syncSsid(this)">)SHHTML";
  html += this->render_network_options_();
  html += R"SHHTML(</select><div class="hint">Nothing showing yet? Give it a few seconds, or enter the network name manually below.</div></div><div class="field"><label for="wifi_ssid">Network name (SSID)</label><input id="wifi_ssid" name="wifi_ssid" maxlength="32" autocomplete="off" placeholder="My Wi-Fi"><div class="wifi-err" id="wifi_err">Choose or type a Wi-Fi network before you save.</div></div><div class="field"><label for="wifi_password">Wi-Fi password</label><div class="pw-wrap"><input id="wifi_password" name="wifi_password" maxlength="64" type="password" autocomplete="current-password" placeholder="Password"><button type="button" class="pw-toggle" id="pw_btn" onclick="togglePw()">Show</button></div></div></div></section><section class="panel"><div class="section-title"><h2>Profile</h2><p>Your firmware is selected automatically from the options below.</p></div>)SHHTML";

  if (this->support_ethernet_) {
    html += R"SHHTML(<div class="choice-grid" id="connection_choices"><label class="choice-card"><input type="radio" name="connection_mode" value="wifi")SHHTML";
    html += (!selected_ethernet ? " checked" : "");
    html += R"SHHTML(><strong>Wi-Fi</strong></label><label class="choice-card"><input type="radio" name="connection_mode" value="ethernet")SHHTML";
    html += (selected_ethernet ? " checked" : "");
    html += R"SHHTML(><strong>Ethernet</strong></label></div><div class="hint eth-hint" id="eth_hint">Still connect Wi-Fi below: the device uses Wi-Fi once to install the Ethernet firmware in the background. Allow at least 2 minutes; once it finishes, the Ethernet port works and you plug in the network cable.<br>No Wi-Fi available, or prefer not to use it? Flash the Ethernet firmware over USB-C at <b>smarthomeshop.io/firmware</b>.</div>)SHHTML";
  } else {
    html += R"SHHTML(<div class="choice-grid"><div class="choice-card active"><strong>Wi-Fi only</strong></div></div><input type="hidden" name="connection_mode" value="wifi">)SHHTML";
  }

  html += R"SHHTML(<input type="hidden" id="firmware_option" name="firmware_option" value=")SHHTML";
  html += html_escape_(selected_firmware);
  html += R"SHHTML("><div class="firmware-summary"><strong>Selected firmware</strong><span id="firmware_summary">)SHHTML";
  html += html_escape_(selected_firmware);
  html += R"SHHTML(</span><span>This is the variant we&rsquo;ll use as the firmware update (OTA) source.</span></div></section><section class="panel"><div class="section-title"><h2>Integrations</h2><p>Home Assistant and Homey work out of the box. Turn SmartHomeShop Cloud and MQTT on or off.</p></div><div class="integrations )SHHTML";
  html += (visible_integrations <= 1 ? "one" : (visible_integrations == 3 ? "" : "two"));
  html += R"SHHTML(">)SHHTML";

  if (this->support_home_assistant_) {
    html += R"SHHTML(<div class="checkcard on locked"><i class="tick">✓</i><span><strong>Home Assistant <em>Always on</em></strong><span>Shows up automatically in Home Assistant on your network. No setup needed.</span></span></div>)SHHTML";
  }
  if (this->support_homey_) {
    html += R"SHHTML(<div class="checkcard on locked"><i class="tick">✓</i><span><strong>Homey <em>Always on</em></strong><span>Add it in Homey with the SmartHomeShop app, locally over your network.</span></span></div>)SHHTML";
  }
  if (this->support_mqtt_) {
    html += R"SHHTML(<label class="checkcard )SHHTML";
    html += (this->settings_.mqtt_enabled != 0 ? "on" : "");
    html += R"SHHTML(" id="mqtt_card"><input type="checkbox" name="mqtt_enabled" id="mqtt_enabled")SHHTML";
    html += this->render_checked_(this->settings_.mqtt_enabled != 0);
    html += R"SHHTML(><i class="tick">✓</i><span><strong>MQTT</strong><span>Send live readings to your own MQTT broker.</span></span></label>)SHHTML";
  }
  if (this->support_cloud_) {
    html += R"SHHTML(<label class="checkcard )SHHTML";
    html += (this->settings_.cloud_enabled != 0 ? "on" : "");
    html += R"SHHTML("><input type="checkbox" name="cloud_enabled" id="cloud_enabled")SHHTML";
    html += this->render_checked_(this->settings_.cloud_enabled != 0);
    html += R"SHHTML(><i class="tick">✓</i><span><strong>SmartHomeShop Cloud</strong><span>Manage this device at app.smarthomeshop.io from your browser.</span></span></label>)SHHTML";
  }
  html += R"SHHTML(</div>)SHHTML";

  if (this->support_mqtt_) {
    html += R"SHHTML(<div class="advanced" id="mqtt_settings"><div class="grid"><div class="field"><label>Broker</label><input name="mqtt_host" maxlength="63" value=")SHHTML";
    html += html_escape_(this->settings_.mqtt_host) + R"SHHTML("></div><div class="field"><label>Port</label><input name="mqtt_port" maxlength="7" value=")SHHTML";
    html += html_escape_(this->settings_.mqtt_port) + R"SHHTML("></div><div class="field"><label>Username</label><input name="mqtt_username" maxlength="47" value=")SHHTML";
    html += html_escape_(this->settings_.mqtt_username) + R"SHHTML("></div><div class="field"><label>Password</label><input name="mqtt_password" maxlength="63" type="password" value=")SHHTML";
    html += html_escape_(this->settings_.mqtt_password) + R"SHHTML("></div><div class="field"><label>Topic prefix</label><input name="mqtt_topic" maxlength="63" value=")SHHTML";
    html += html_escape_(this->settings_.mqtt_topic) + R"SHHTML("></div></div></div>)SHHTML";
  }

  html += R"SHHTML(<div class="actions"><button class="btn block" type="submit">Save and connect</button></div></form><p class="micro">Once you save, the device connects straight away. It&rsquo;s normal for the fallback Wi-Fi network to disappear. <a href="https://docs.smarthomeshop.io" target="_blank" rel="noreferrer">Open documentation</a></p></section></main><script>
var firmwareOptions=)SHHTML";
  html += firmware_options_json;
  html += R"SHHTML(;
var fallbackFirmware=")SHHTML";
  html += json_escape_(selected_firmware);
  html += R"SHHTML(";
function norm(v){return (v||'').toLowerCase();}
function syncSsid(sel){if(sel&&sel.value){document.getElementById('wifi_ssid').value=sel.value;}}
function togglePw(){var i=document.getElementById('wifi_password');var b=document.getElementById('pw_btn');if(!i||!b)return;var s=i.type==='password';i.type=s?'text':'password';b.textContent=s?'Hide':'Show';}
function syncCard(input){var card=input.closest('.checkcard');if(card){card.classList.toggle('on',input.checked);}}
function syncChoices(){document.querySelectorAll('.choice-card input[type="radio"]').forEach(function(input){var card=input.closest('.choice-card');if(card){card.classList.toggle('active',input.checked);}});var h=document.getElementById('eth_hint');if(h)h.classList.toggle('show',selectedConnection()==='ethernet');}
function selectedConnection(){var r=document.querySelector('input[name="connection_mode"]:checked');return r?r.value:'wifi';}
function selectedProfile(){var n=norm(fallbackFirmware);if(n.indexOf('complete')>=0)return 'complete';if(n.indexOf('basic')>=0)return 'basic';return '';}
function firmwareMatches(option,connection,cloud,profile,requireProfile){var n=norm(option);var isEth=n.indexOf('ethernet')>=0;var isWifi=n.indexOf('wifi')>=0||!isEth;var isCloud=n.indexOf('cloud')>=0||n.indexOf('app')>=0;var okConnection=connection==='ethernet'?isEth:isWifi;var okCloud=cloud?isCloud:!isCloud;var okProfile=!requireProfile||!profile||n.indexOf(profile)>=0;return okConnection&&okCloud&&okProfile;}
function chooseFirmware(){var hidden=document.getElementById('firmware_option');var summary=document.getElementById('firmware_summary');var cloud=document.getElementById('cloud_enabled');var connection=selectedConnection();var cloudOn=cloud?cloud.checked:false;var profile=selectedProfile();var list=firmwareOptions.length?firmwareOptions:[fallbackFirmware];var chosen=list.find(function(o){return firmwareMatches(o,connection,cloudOn,profile,true);})||list.find(function(o){return firmwareMatches(o,connection,cloudOn,profile,false);})||list.find(function(o){return firmwareMatches(o,'wifi',cloudOn,profile,true);})||list.find(function(o){return firmwareMatches(o,'wifi',cloudOn,profile,false);})||fallbackFirmware;if(hidden)hidden.value=chosen;if(summary)summary.textContent=chosen;}
function syncMqtt(){var input=document.getElementById('mqtt_enabled');var settings=document.getElementById('mqtt_settings');if(settings){settings.classList.toggle('show',!!(input&&input.checked));}}
function effectiveSsid(){var m=document.getElementById('wifi_ssid');var s=document.getElementById('wifi_select');var v=(m&&m.value?m.value:(s?s.value:'')).trim();return v;}
function clearWifiError(){var e=document.getElementById('wifi_err');var m=document.getElementById('wifi_ssid');if(e)e.classList.remove('show');if(m)m.classList.remove('invalid');}
function validateWifi(){if(effectiveSsid())return true;var e=document.getElementById('wifi_err');var m=document.getElementById('wifi_ssid');if(e)e.classList.add('show');if(m){m.classList.add('invalid');m.focus();m.scrollIntoView({block:'center',behavior:'smooth'});}return false;}
document.querySelectorAll('.checkcard input').forEach(function(input){input.addEventListener('change',function(){syncCard(input);syncMqtt();chooseFirmware();});syncCard(input);});document.querySelectorAll('.choice-card input[type="radio"]').forEach(function(input){input.addEventListener('change',function(){syncChoices();chooseFirmware();});});
var wifiIn=document.getElementById('wifi_ssid');if(wifiIn)wifiIn.addEventListener('input',clearWifiError);var wifiSel=document.getElementById('wifi_select');if(wifiSel)wifiSel.addEventListener('change',clearWifiError);
var setupForm=document.getElementById('setup_form');if(setupForm)setupForm.addEventListener('submit',function(e){if(!validateWifi())e.preventDefault();});
syncChoices();syncMqtt();chooseFirmware();
</script></body></html>)SHHTML";

  this->send_response_(request, 200, "text/html; charset=utf-8", html);
}

void SmartHomeShopSetupPortal::send_saved_page_(AsyncWebServerRequest *request) {
  std::string html;
  html.reserve(11000);
  const bool cloud_on = this->cloud_enabled();
  const std::string local_ip = this->local_ip_();
  html += R"SHHTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">)SHHTML";
  html += R"SHHTML(<title>Settings saved</title><style>
:root{--ink:#0a0a0a;--muted:#6b7280;--bg:#f7f9fb;--card:#ffffff;--line:#e5e7eb;--blue:#2563eb;--blue-dark:#1d4ed8;--brand:linear-gradient(135deg,#338cf5,#4fd1c5);--shadow:0 1px 2px rgba(16,24,40,.05),0 1px 3px rgba(16,24,40,.06)}*{box-sizing:border-box}body{margin:0;min-height:100svh;padding:env(safe-area-inset-top) 16px calc(24px + env(safe-area-inset-bottom));display:grid;place-items:center;font-family:ui-sans-serif,system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;color:var(--ink);background:linear-gradient(180deg,#ffffff 0%,var(--bg) 40%,var(--bg) 100%);-webkit-font-smoothing:antialiased}.card{width:min(620px,100%);border:1px solid var(--line);border-radius:20px;background:var(--card);box-shadow:var(--shadow);padding:clamp(22px,6vw,36px);position:relative;overflow:hidden}.card:after{content:"";position:absolute;inset:0 0 auto;height:4px;background:linear-gradient(90deg,#338cf5,#4fd1c5)}.brand{display:flex;align-items:center;gap:12px;margin-bottom:24px}.mark{width:42px;height:42px;border-radius:12px;background:var(--brand);display:grid;place-items:center;flex:0 0 auto}.mark svg{width:24px;height:19px;display:block}.brand strong{display:block;font-size:16px;font-weight:600;letter-spacing:-.01em}.eyebrow{text-transform:uppercase;letter-spacing:.14em;color:var(--blue);font-size:11px;font-weight:700}h1{margin:14px 0 10px;font-size:clamp(26px,7vw,36px);line-height:1.1;letter-spacing:-.03em;font-weight:700}p{font-size:15px;line-height:1.55;color:var(--muted)}.pulse{width:10px;height:10px;border-radius:50%;background:#10b981;box-shadow:0 0 0 0 rgba(16,185,129,.45);animation:p 1.4s infinite}@keyframes p{to{box-shadow:0 0 0 16px rgba(16,185,129,0)}}.status{display:inline-flex;align-items:center;gap:9px;border:1px solid #a7f3d0;border-radius:999px;background:#ecfdf5;padding:7px 13px;font-weight:600;font-size:13px;color:#065f46}.summary{display:grid;gap:8px;margin:18px 0}.summary div{display:flex;justify-content:space-between;gap:16px;border:1px solid var(--line);border-radius:12px;background:#fff;padding:12px 14px;font-size:14px;color:var(--muted)}.summary strong{color:var(--ink);font-weight:600}.actions{display:grid;gap:10px;margin-top:18px}.btn,.ghost{border-radius:12px;min-height:48px;padding:13px 18px;font-weight:600;font-size:15px;text-decoration:none;display:inline-flex;align-items:center;justify-content:center}.btn{background:var(--blue);color:#fff;box-shadow:0 12px 26px -20px rgba(37,99,235,.65)}.btn:hover{background:var(--blue-dark)}.ghost{border:1px solid var(--line);color:var(--blue-dark);background:#fff}.block{width:100%}.ha-block{margin:18px 0 0;border:1px solid var(--line);border-radius:14px;background:#f9fafb;padding:16px}.ha-block>strong{display:block;font-size:15px;font-weight:600;color:var(--ink);margin-bottom:10px}.ha-block ol{margin:0;padding-left:20px;color:var(--muted);font-size:14px;line-height:1.6}.ha-block ol b{color:var(--ink);font-weight:600}.ipbox{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-top:14px;border:1px solid #b9e6df;border-radius:12px;background:#f0fbf9;padding:12px 14px}.ipbox small{display:block;text-transform:uppercase;letter-spacing:.08em;font-size:11px;font-weight:700;color:#0f766e}.ipbox b{font-size:18px;font-weight:700;color:#134e4a;font-variant-numeric:tabular-nums}.ipbox.waiting b{font-size:14px;font-weight:500;color:#5f7470}.copy-ip{appearance:none;border:1px solid #b9e6df;background:#fff;border-radius:8px;padding:7px 12px;font:inherit;font-size:12.5px;font-weight:600;color:#0f766e;cursor:pointer}.micro{margin:14px 0 0;text-align:center;color:var(--muted);font-size:12.5px;line-height:1.5}@media(max-width:560px){.summary div{display:block}.summary span{display:block;margin-top:3px}}
</style></head><body><main class="card"><div class="brand"><div class="mark">)SHHTML";
  html += BRAND_MARK_SVG;
  html += R"SHHTML(</div><div><div class="eyebrow">Device setup</div><strong>)SHHTML";
  html += html_escape_(this->product_name_);
  html += R"SHHTML(</strong></div></div><div class="status"><i class="pulse"></i>Settings saved</div><h1>)SHHTML";
  html += (this->pending_wifi_ ? "Your device is connecting." : "You&rsquo;re all set.");
  html += R"SHHTML(</h1><p>)SHHTML";
  html += (this->pending_wifi_ ? "We&rsquo;ve got your Wi-Fi details. The device is testing the connection now and only keeps them once it works. Your phone may drop the fallback network." : "Your settings are saved on the device. You can close this window or head back to setup.");
  html += R"SHHTML(</p><div class="summary"><div><strong>Firmware</strong><span>)SHHTML";
  html += html_escape_(this->firmware_option());
  html += R"SHHTML(</span></div><div><strong>SmartHomeShop Cloud</strong><span>)SHHTML";
  html += (cloud_on ? "on" : "off");
  if (this->support_home_assistant_) {
    html += R"SHHTML(</span></div><div><strong>Home Assistant</strong><span>)SHHTML";
    html += (this->home_assistant_enabled() ? "on" : "off");
  }
  if (this->support_homey_) {
    html += R"SHHTML(</span></div><div><strong>Homey</strong><span>)SHHTML";
    html += (this->homey_enabled() ? "on" : "off");
  }
  if (this->support_mqtt_) {
    html += R"SHHTML(</span></div><div><strong>MQTT</strong><span>)SHHTML";
    html += (this->mqtt_enabled() ? "on" : "off");
  }
  html += R"SHHTML(</span></div></div>)SHHTML";
  const std::string saved_firmware = this->firmware_option();
  if (this->support_ethernet_ && saved_firmware.find("Ethernet") != std::string::npos) {
    html += R"SHHTML(<div class="ha-block"><strong>Ethernet firmware is installing</strong><ol><li>Keep the device powered. The Ethernet firmware installs in the background; allow at least 2 minutes.</li><li>The device restarts on its own when the installation is done.</li><li>Plug in the network cable. The Ethernet port works from then on.</li></ol></div>)SHHTML";
  }
  if (cloud_on) {
    html += R"SHHTML(<div class="ha-block"><strong>Finish setup in your browser</strong><ol><li>Close this window (tap the cross at the top).</li><li>Reconnect your phone or computer to the same Wi-Fi network the device just joined.</li><li>Go to <b>app.smarthomeshop.io/start</b> in your browser to create an account and link this device.</li></ol></div><p class="micro">This setup window has no internet, so open the link once you are back on your normal network. Prefer to stay local? <a href="https://docs.smarthomeshop.io" target="_blank" rel="noreferrer">Read the documentation</a>.</p>)SHHTML";
  } else {
    html += R"SHHTML(<div class="ha-block"><strong>Add it to Home Assistant</strong><ol><li>Open Home Assistant on your phone or computer.</li><li>Go to <b>Settings &rarr; Devices &amp; services</b>.</li><li>Your device appears there automatically as a discovered <b>ESPHome</b> device. Click <b>Configure</b> to add it.</li></ol><div class="ipbox)SHHTML";
    html += (local_ip.empty() ? " waiting" : "");
    html += R"SHHTML(" id="ipbox"><span><small>Local IP address</small><b id="dev_ip">)SHHTML";
    html += (local_ip.empty() ? std::string("Finding IP address&hellip;") : html_escape_(local_ip));
    html += R"SHHTML(</b></span><button type="button" class="copy-ip" id="copy_ip")SHHTML";
    html += (local_ip.empty() ? " hidden" : "");
    html += R"SHHTML( onclick="copyIp()">Copy</button></div></div><div class="actions"><a class="btn block" href="https://docs.smarthomeshop.io" target="_blank" rel="noreferrer">Open documentation</a></div><script>
function copyIp(){var b=document.getElementById('dev_ip');if(!b)return;var t=(b.textContent||'').trim();if(navigator.clipboard&&/^\d+\.\d+\.\d+\.\d+$/.test(t)){navigator.clipboard.writeText(t);var c=document.getElementById('copy_ip');if(c){c.textContent='Copied';setTimeout(function(){c.textContent='Copy';},1500);}}}
(function(){var tries=0;var b=document.getElementById('dev_ip');if(!b)return;if(/^\d+\.\d+\.\d+\.\d+$/.test((b.textContent||'').trim()))return;function poll(){tries++;fetch('/setup/status',{cache:'no-store'}).then(function(r){return r.json();}).then(function(d){if(d&&d.ip){b.textContent=d.ip;var box=document.getElementById('ipbox');if(box)box.classList.remove('waiting');var c=document.getElementById('copy_ip');if(c)c.hidden=false;return;}if(tries<40)setTimeout(poll,2000);}).catch(function(){if(tries<40)setTimeout(poll,2000);});}poll();})();
</script>)SHHTML";
  }
  html += R"SHHTML(</main></body></html>)SHHTML";
  this->send_response_(request, 200, "text/html; charset=utf-8", html);
}

void SmartHomeShopSetupPortal::send_status_(AsyncWebServerRequest *request) {
  std::string json;
  json.reserve(512);
  json += "{\"status\":\"" + json_escape_(this->status_text_()) + "\"";
  json += ",\"cloud_enabled\":" + std::string(this->cloud_enabled() ? "true" : "false");
  json += ",\"home_assistant_enabled\":" + std::string(this->home_assistant_enabled() ? "true" : "false");
  json += ",\"homey_enabled\":" + std::string(this->homey_enabled() ? "true" : "false");
  json += ",\"mqtt_enabled\":" + std::string(this->mqtt_enabled() ? "true" : "false");
  json += ",\"firmware_option\":\"" + json_escape_(this->firmware_option()) + "\"";
  json += ",\"pending_wifi\":" + std::string(this->pending_wifi_ ? "true" : "false");
  json += ",\"applying_wifi\":" + std::string(this->wifi_apply_active_ ? "true" : "false");
  json += ",\"ip\":\"" + json_escape_(this->local_ip_()) + "\"";
  json += "}";
  this->send_response_(request, 200, "application/json; charset=utf-8", json);
}

bool SmartHomeShopSetupPortal::save_from_request_(AsyncWebServerRequest *request) {
  std::string ssid = this->bounded_arg_(request, "wifi_ssid_select", 32);
  if (ssid.empty())
    ssid = this->bounded_arg_(request, "wifi_ssid", 32);
  const std::string password = this->bounded_arg_(request, "wifi_password", 64);

  // A Wi-Fi network is always mandatory: the portal only runs on Wi-Fi
  // firmware, and even when the user picks Ethernet the device needs Wi-Fi
  // to download the Ethernet firmware over OTA first. Reject the submit
  // without changing anything so the form can be corrected.
  if (ssid.empty()) {
    this->last_notice_ = "Choose or type a Wi-Fi network before you save.";
    return false;
  }

  this->settings_.cloud_enabled = this->support_cloud_ && request->hasArg("cloud_enabled") ? 1 : 0;
  // Home Assistant (native API) is always on when supported; it is not a toggle.
  this->settings_.home_assistant_enabled = this->support_home_assistant_ ? 1 : 0;
  this->settings_.mqtt_enabled = this->support_mqtt_ && request->hasArg("mqtt_enabled") ? 1 : 0;
  this->copy_to_(this->settings_.mqtt_host, sizeof(this->settings_.mqtt_host), this->bounded_arg_(request, "mqtt_host", 63));
  this->copy_to_(this->settings_.mqtt_port, sizeof(this->settings_.mqtt_port), this->bounded_arg_(request, "mqtt_port", 7));
  this->copy_to_(this->settings_.mqtt_username, sizeof(this->settings_.mqtt_username), this->bounded_arg_(request, "mqtt_username", 47));
  this->copy_to_(this->settings_.mqtt_password, sizeof(this->settings_.mqtt_password), this->bounded_arg_(request, "mqtt_password", 63));
  this->copy_to_(this->settings_.mqtt_topic, sizeof(this->settings_.mqtt_topic), this->bounded_arg_(request, "mqtt_topic", 63));
  if (this->support_firmware_selector_) {
    std::string firmware_option = this->bounded_arg_(request, "firmware_option", 95);
    if (firmware_option.empty())
      firmware_option = !this->default_firmware_option_.empty() ? this->default_firmware_option_ : this->product_variant_;
    this->copy_to_(this->settings_.firmware_option, sizeof(this->settings_.firmware_option), firmware_option);
    this->settings_.firmware_dirty = 1;
  }
  this->save_settings_();

  if (!ssid.empty()) {
    this->pending_wifi_ssid_ = ssid;
    this->pending_wifi_password_ = password;
    this->pending_wifi_ = true;
    this->wifi_apply_active_ = false;
    this->wifi_last_error_ = false;
    this->last_notice_ = "Settings saved. Testing your Wi-Fi now.";
  } else {
    this->last_notice_ = "Settings saved. Wi-Fi was left unchanged.";
  }
  return true;
}

std::string SmartHomeShopSetupPortal::bounded_arg_(AsyncWebServerRequest *request, const char *name, size_t max_length) {
  if (request == nullptr || !request->hasArg(name))
    return {};
  std::string value = request->arg(name);
  if (value.size() > max_length)
    value.resize(max_length);
  return value;
}

void SmartHomeShopSetupPortal::copy_to_(char *dest, size_t len, const std::string &value) {
  if (dest == nullptr || len == 0)
    return;
  std::snprintf(dest, len, "%s", value.c_str());
}

std::string SmartHomeShopSetupPortal::html_escape_(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (char c : value) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out.push_back(c); break;
    }
  }
  return out;
}

std::string SmartHomeShopSetupPortal::json_escape_(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (char c : value) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out.push_back(c); break;
    }
  }
  return out;
}

std::string SmartHomeShopSetupPortal::wifi_ap_ssid_(const wifi::WiFiAP &ap) {
  auto ssid = ap.get_ssid();
  return std::string(ssid.c_str(), ssid.size());
}

std::string SmartHomeShopSetupPortal::wifi_ap_password_(const wifi::WiFiAP &ap) {
  auto password = ap.get_password();
  return std::string(password.c_str(), password.size());
}

}  // namespace smarthomeshop_setup_portal
}  // namespace esphome
