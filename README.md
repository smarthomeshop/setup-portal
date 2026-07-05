# SmartHomeShop Setup Portal

Internal SmartHomeShop package used for device onboarding on our ESPHome-based
products.

## Usage

```yaml
packages:
  setup_portal: github://smarthomeshop/setup-portal/packages/setup-portal.yaml@main
```

Configuration is done through the `setup_*` substitutions, see
`packages/setup-portal.yaml` and `examples/` for a minimal config. Requires a
WiFi firmware with `captive_portal:`.

For internal use in SmartHomeShop products. No support is provided for use
outside SmartHomeShop firmware.
