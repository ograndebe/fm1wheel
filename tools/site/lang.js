/* Remember the language of the page being read, and of a language link the reader chooses.
 * The root page uses the saved value on the next visit. */
(() => {
  const KEY = "fm1-wheel-lang";
  const pageLang = document.documentElement.lang.toLowerCase().startsWith("pt") ? "pt" : "en";
  try { localStorage.setItem(KEY, pageLang); } catch (_) {}
  for (const a of document.querySelectorAll("a[data-lang]")) {
    a.addEventListener("click", () => {
      try { localStorage.setItem(KEY, a.dataset.lang); } catch (_) {}
    });
  }
})();
