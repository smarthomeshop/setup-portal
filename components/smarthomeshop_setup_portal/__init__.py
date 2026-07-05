import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["web_server_base", "wifi"]
AUTO_LOAD = ["web_server_base", "json"]

smarthomeshop_ns = cg.esphome_ns.namespace("smarthomeshop_setup_portal")
SmartHomeShopSetupPortal = smarthomeshop_ns.class_("SmartHomeShopSetupPortal", cg.Component)

CONF_PRODUCT_NAME = "product_name"
CONF_PRODUCT_VARIANT = "product_variant"
CONF_SUPPORT_CLOUD = "support_cloud"
CONF_SUPPORT_HOME_ASSISTANT = "support_home_assistant"
CONF_SUPPORT_MQTT = "support_mqtt"
CONF_SUPPORT_ETHERNET = "support_ethernet"
CONF_SUPPORT_FIRMWARE_SELECTOR = "support_firmware_selector"
CONF_DEFAULT_CLOUD_ENABLED = "default_cloud_enabled"
CONF_DEFAULT_HOME_ASSISTANT_ENABLED = "default_home_assistant_enabled"
CONF_DEFAULT_MQTT_ENABLED = "default_mqtt_enabled"
CONF_FIRMWARE_OPTIONS = "firmware_options"
CONF_DEFAULT_FIRMWARE_OPTION = "default_firmware_option"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SmartHomeShopSetupPortal),
        cv.Optional(CONF_PRODUCT_NAME, default="SmartHomeShop device"): cv.string,
        cv.Optional(CONF_PRODUCT_VARIANT, default="ESPHome firmware"): cv.string,
        cv.Optional(CONF_SUPPORT_CLOUD, default=True): cv.boolean,
        cv.Optional(CONF_SUPPORT_HOME_ASSISTANT, default=True): cv.boolean,
        cv.Optional(CONF_SUPPORT_MQTT, default=True): cv.boolean,
        cv.Optional(CONF_SUPPORT_ETHERNET, default=False): cv.boolean,
        cv.Optional(CONF_SUPPORT_FIRMWARE_SELECTOR, default=False): cv.boolean,
        cv.Optional(CONF_DEFAULT_CLOUD_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_DEFAULT_HOME_ASSISTANT_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_DEFAULT_MQTT_ENABLED, default=False): cv.boolean,
        cv.Optional(CONF_FIRMWARE_OPTIONS, default=""): cv.string,
        cv.Optional(CONF_DEFAULT_FIRMWARE_OPTION, default=""): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_product_name(config[CONF_PRODUCT_NAME]))
    cg.add(var.set_product_variant(config[CONF_PRODUCT_VARIANT]))
    cg.add(var.set_support_cloud(config[CONF_SUPPORT_CLOUD]))
    cg.add(var.set_support_home_assistant(config[CONF_SUPPORT_HOME_ASSISTANT]))
    cg.add(var.set_support_mqtt(config[CONF_SUPPORT_MQTT]))
    cg.add(var.set_support_ethernet(config[CONF_SUPPORT_ETHERNET]))
    cg.add(var.set_support_firmware_selector(config[CONF_SUPPORT_FIRMWARE_SELECTOR]))
    cg.add(var.set_default_cloud_enabled(config[CONF_DEFAULT_CLOUD_ENABLED]))
    cg.add(var.set_default_home_assistant_enabled(config[CONF_DEFAULT_HOME_ASSISTANT_ENABLED]))
    cg.add(var.set_default_mqtt_enabled(config[CONF_DEFAULT_MQTT_ENABLED]))
    cg.add(var.set_firmware_options(config[CONF_FIRMWARE_OPTIONS]))
    cg.add(var.set_default_firmware_option(config[CONF_DEFAULT_FIRMWARE_OPTION]))
