(() => {
  const root = document.getElementById("root");
  if (!root) return;

  async function postJson(path, body) {
    const response = await fetch(path, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    const result = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(result.error || `Request failed (${response.status})`);
    }
    return result;
  }

  function addStudentDeleteButtons() {
    const title = root.querySelector(".page-heading h1")?.textContent?.trim().toLowerCase() || "";
    if (!title.includes("student")) return;

    for (const row of root.querySelectorAll(".table-scroll tbody tr")) {
      const uid = row.querySelector(".uid-chip")?.textContent?.trim();
      const actionCell = row.lastElementChild;
      if (!uid || !actionCell || actionCell.querySelector(".data-delete-button")) continue;

      const button = document.createElement("button");
      button.type = "button";
      button.className = "data-delete-button";
      button.textContent = "Delete";
      button.setAttribute("aria-label", "Delete student and their saved history");
      button.addEventListener("click", async (event) => {
        event.preventDefault();
        event.stopPropagation();
        const name = row.querySelector(".person-chip strong")?.textContent?.trim() || "this student";
        const confirmed = window.confirm(
          `Permanently delete ${name} and their saved history? A student with a bag assigned must collect it first.`
        );
        if (!confirmed) return;

        button.disabled = true;
        button.textContent = "Deleting…";
        try {
          await postJson("/api/students/delete", { rfidUid: uid });
          window.location.reload();
        } catch (error) {
          button.disabled = false;
          button.textContent = "Delete";
          window.alert(error instanceof Error ? error.message : "Could not delete the student.");
        }
      });
      actionCell.appendChild(button);
    }
  }

  function addHistoryClearButton() {
    const title = root.querySelector(".page-heading h1")?.textContent?.trim().toLowerCase() || "";
    if (!title.includes("activity") && !title.includes("history")) return;

    const toolbar = root.querySelector(".page-content .page-stack .toolbar");
    if (!toolbar || toolbar.querySelector(".history-clear-button")) return;

    const button = document.createElement("button");
    button.type = "button";
    button.className = "history-clear-button";
    button.textContent = "Clear completed history";
    button.addEventListener("click", async () => {
      const confirmed = window.confirm(
        "Delete all completed activity records? Active bag assignments and their records will be kept."
      );
      if (!confirmed) return;

      button.disabled = true;
      button.textContent = "Clearing…";
      try {
        const result = await postJson("/api/history/clear-completed", {});
        window.alert(`${result.removed || 0} completed record(s) removed. Active assignments were kept.`);
        window.location.reload();
      } catch (error) {
        button.disabled = false;
        button.textContent = "Clear completed history";
        window.alert(error instanceof Error ? error.message : "Could not clear history.");
      }
    });
    toolbar.appendChild(button);
  }

  let updateScheduled = false;
  const refreshControls = () => {
    if (updateScheduled) return;
    updateScheduled = true;
    window.requestAnimationFrame(() => {
      updateScheduled = false;
      addStudentDeleteButtons();
      addHistoryClearButton();
    });
  };

  new MutationObserver(refreshControls).observe(root, { childList: true, subtree: true });
  refreshControls();
})();

