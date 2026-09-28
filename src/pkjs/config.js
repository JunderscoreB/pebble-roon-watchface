module.exports = [
  {
    "type": "heading",
    "defaultValue": "Roon Remote Config"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Bridge Connection"
      },
      {
        "type": "input",
        "messageKey": "bridge_ip",
        "defaultValue": "192.168.1.50",
        "label": "Bridge IP Address"
      },
      {
        "type": "input",
        "messageKey": "bridge_port",
        "defaultValue": "3000",
        "label": "Bridge Port"
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Watchface UI"
      },
      {
        "type": "select",
        "messageKey": "font_size",
        "defaultValue": "1",
        "label": "Font Size",
        "options": [
          { "label": "Small", "value": "0" },
          { "label": "Medium", "value": "1" },
          { "label": "Large", "value": "2" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "scroll_text",
        "defaultValue": false,
        "label": "Marquee Scrolling Text"
      },
      {
        "type": "toggle",
        "messageKey": "enable_touch",
        "defaultValue": true,
        "label": "Enable Touch Gestures"
      },
      {
        "type": "toggle",
        "messageKey": "respect_quiet_time",
        "defaultValue": true,
        "label": "Respect Quiet Time"
      },
      {
        "type": "select",
        "messageKey": "theme",
        "defaultValue": "0",
        "label": "Theme",
        "options": [
          { "label": "Dark", "value": "0" },
          { "label": "Light", "value": "1" }
        ]
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];