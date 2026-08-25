/** In-app dialogs — Tauri remaps/blocks window.confirm and window.prompt. */

let confirmActionResolver = null;

export function studioConfirm({
  title = "Confirm",
  message,
  confirmLabel = "OK",
  danger = false,
} = {}) {
  return new Promise((resolve) => {
    const overlay = document.getElementById("confirm-action-overlay");
    const titleEl = document.getElementById("confirm-action-title");
    const msgEl = document.getElementById("confirm-action-message");
    const okBtn = document.getElementById("confirm-action-ok");
    if (!overlay || !msgEl || !okBtn) {
      resolve(false);
      return;
    }
    if (confirmActionResolver) {
      confirmActionResolver(false);
      confirmActionResolver = null;
    }
    confirmActionResolver = resolve;
    if (titleEl) titleEl.textContent = title;
    msgEl.textContent = message || "";
    okBtn.textContent = confirmLabel;
    okBtn.classList.toggle("danger", !!danger);
    okBtn.classList.toggle("success", !danger);
    overlay.classList.remove("hidden");
    okBtn.focus();
  });
}

export function finishStudioConfirm(result) {
  document.getElementById("confirm-action-overlay")?.classList.add("hidden");
  const resolve = confirmActionResolver;
  confirmActionResolver = null;
  if (resolve) resolve(!!result);
}

export function bindStudioConfirmUi() {
  document.getElementById("confirm-action-ok")?.addEventListener("click", () =>
    finishStudioConfirm(true)
  );
  document
    .getElementById("confirm-action-cancel")
    ?.addEventListener("click", () => finishStudioConfirm(false));
  document.getElementById("confirm-action-overlay")?.addEventListener("click", (e) => {
    if (e.target.id === "confirm-action-overlay") finishStudioConfirm(false);
  });
  document.addEventListener("keydown", (e) => {
    if (e.key !== "Escape") return;
    const overlay = document.getElementById("confirm-action-overlay");
    if (overlay && !overlay.classList.contains("hidden")) {
      finishStudioConfirm(false);
    }
  });
}
