/* pureikyubu GitHub Pages — light/dark theme switch.
   The current theme is applied by a tiny inline script in <head> (no flash);
   this file only wires up the toggle button and remembers the choice. */

(function () {
  "use strict";

  var STORAGE_KEY = "pureikyubu-theme";

  var LABELS = {
    en: { light: "☀ Light", dark: "☾ Dark",
          toLight: "Switch to the light theme", toDark: "Switch to the dark theme" },
    ru: { light: "☀ Светлая", dark: "☾ Тёмная",
          toLight: "Включить светлую тему", toDark: "Включить тёмную тему" }
  };

  function labels() {
    var lang = (document.documentElement.lang || "en").slice(0, 2).toLowerCase();
    return LABELS[lang] || LABELS.en;
  }

  function storedTheme() {
    try {
      var value = window.localStorage.getItem(STORAGE_KEY);
      return value === "light" || value === "dark" ? value : null;
    } catch (e) {
      return null;
    }
  }

  function currentTheme() {
    return document.documentElement.getAttribute("data-theme") || "dark";
  }

  function applyTheme(theme, remember) {
    document.documentElement.setAttribute("data-theme", theme);
    if (remember) {
      try { window.localStorage.setItem(STORAGE_KEY, theme); } catch (e) { /* private mode */ }
    }
    updateButtons(theme);
  }

  function updateButtons(theme) {
    var buttons = document.querySelectorAll("[data-theme-toggle]");
    var text = labels();
    for (var i = 0; i < buttons.length; i++) {
      var button = buttons[i];
      var toLight = theme !== "light";
      button.textContent = toLight ? text.light : text.dark;
      button.setAttribute("aria-label", toLight ? text.toLight : text.toDark);
      button.setAttribute("title", button.getAttribute("aria-label"));
    }
  }

  function init() {
    updateButtons(currentTheme());

    document.addEventListener("click", function (event) {
      var button = event.target.closest ? event.target.closest("[data-theme-toggle]") : null;
      if (!button) { return; }
      event.preventDefault();
      applyTheme(currentTheme() === "light" ? "dark" : "light", true);
    });

    // Follow the system preference until the visitor makes an explicit choice.
    if (window.matchMedia && !storedTheme()) {
      var query = window.matchMedia("(prefers-color-scheme: light)");
      var onChange = function (event) {
        if (!storedTheme()) { applyTheme(event.matches ? "light" : "dark", false); }
      };
      if (query.addEventListener) { query.addEventListener("change", onChange); }
      else if (query.addListener) { query.addListener(onChange); }
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
