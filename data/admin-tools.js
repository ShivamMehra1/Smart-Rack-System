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

  async function getJson(path) {
    const response = await fetch(path, { cache: "no-store" });
    const result = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(result.error || `Request failed (${response.status})`);
    return result;
  }

  function onCompartmentsPage() {
    const title = root.querySelector(".page-heading h1")?.textContent?.trim().toLowerCase() || "";
    return title.includes("compartment");
  }

  function makePreview(rows, cols, format, container) {
    container.replaceChildren();
    const previewRows = Math.min(rows, 3);
    const previewCols = Math.min(cols, 4);
    for (let row = 0; row < previewRows; row += 1) {
      const line = document.createElement("div");
      line.className = "rack-layout-preview-row";
      for (let col = 0; col < previewCols; col += 1) {
        const chip = document.createElement("span");
        chip.className = "rack-layout-preview-chip";
        chip.textContent = format === "ALPHA_NUM"
          ? `${String.fromCharCode(65 + row)}${col + 1}`
          : String((row + 1) * 100 + col + 1);
        line.appendChild(chip);
      }
      container.appendChild(line);
    }
  }

  async function openRackLayoutDialog() {
    if (document.querySelector(".rack-layout-backdrop")) return;
    let layout;
    try {
      layout = await getJson("/api/compartments/layout");
    } catch (error) {
      window.alert(error instanceof Error ? error.message : "Could not load rack settings.");
      return;
    }

    const backdrop = document.createElement("div");
    backdrop.className = "rack-layout-backdrop";
    backdrop.innerHTML = `
      <form class="rack-layout-dialog" role="dialog" aria-modal="true" aria-labelledby="rack-layout-title">
        <div class="rack-layout-heading">
          <div><span class="eyebrow">COMPARTMENT SETUP</span><h2 id="rack-layout-title">Customize rack layout</h2></div>
          <button type="button" class="rack-layout-close" aria-label="Close">×</button>
        </div>
        <p class="rack-layout-description">Set the physical grid and the label format shown in the rack, student records, and collection prompts.</p>
        <div class="rack-layout-fields">
          <label>Rows<input name="rows" type="number" min="1" max="100" required></label>
          <label>Columns<input name="cols" type="number" min="1" max="100" required></label>
          <label class="rack-layout-format">Compartment labels
            <select name="namingFormat">
              <option value="THREE_DIGIT">Row and column numbers (101, 102, 201…)</option>
              <option value="ALPHA_NUM">Letters and numbers (A1, A2, B1…)</option>
            </select>
          </label>
        </div>
        <div class="rack-layout-preview"><div><span class="eyebrow">LABEL PREVIEW</span><strong class="rack-layout-count"></strong></div><div class="rack-layout-preview-grid"></div></div>
        <p class="rack-layout-note">Up to 100 compartments. Existing occupied or maintenance compartments must fit inside the new grid; student records and completed history are retained.</p>
        <p class="rack-layout-error" role="alert" hidden></p>
        <div class="rack-layout-actions"><button type="button" class="rack-layout-cancel">Cancel</button><button type="submit" class="rack-layout-save">Save layout</button></div>
      </form>`;
    document.body.appendChild(backdrop);

    const form = backdrop.querySelector("form");
    const rowsInput = form.elements.rows;
    const colsInput = form.elements.cols;
    const formatInput = form.elements.namingFormat;
    const preview = form.querySelector(".rack-layout-preview-grid");
    const count = form.querySelector(".rack-layout-count");
    const errorBox = form.querySelector(".rack-layout-error");
    const saveButton = form.querySelector(".rack-layout-save");
    rowsInput.value = String(layout.rows || 4);
    colsInput.value = String(layout.cols || 6);
    formatInput.value = layout.namingFormat === "ALPHA_NUM" ? "ALPHA_NUM" : "THREE_DIGIT";

    const close = () => backdrop.remove();
    form.querySelector(".rack-layout-close").addEventListener("click", close);
    form.querySelector(".rack-layout-cancel").addEventListener("click", close);
    backdrop.addEventListener("click", (event) => { if (event.target === backdrop) close(); });

    const updatePreview = () => {
      const rows = Number(rowsInput.value);
      const cols = Number(colsInput.value);
      const format = formatInput.value;
      const valid = Number.isInteger(rows) && Number.isInteger(cols) && rows > 0 && cols > 0 && rows * cols <= 100 &&
        (format !== "ALPHA_NUM" || rows <= 26);
      errorBox.hidden = valid;
      errorBox.textContent = rows * cols > 100
        ? "The rack can contain at most 100 compartments."
        : format === "ALPHA_NUM" && rows > 26
          ? "Letter-number labels support up to 26 rows (A–Z)."
          : "Enter a valid row and column count.";
      saveButton.disabled = !valid;
      if (valid) {
        count.textContent = `${rows} × ${cols} · ${rows * cols} compartments`;
        makePreview(rows, cols, format, preview);
      } else {
        count.textContent = "Layout preview";
        preview.replaceChildren();
      }
    };
    rowsInput.addEventListener("input", updatePreview);
    colsInput.addEventListener("input", updatePreview);
    formatInput.addEventListener("change", updatePreview);
    updatePreview();

    form.addEventListener("submit", async (event) => {
      event.preventDefault();
      errorBox.hidden = true;
      saveButton.disabled = true;
      saveButton.textContent = "Saving…";
      try {
        const result = await postJson("/api/compartments/layout", {
          rows: Number(rowsInput.value),
          cols: Number(colsInput.value),
          namingFormat: formatInput.value,
        });
        close();
        window.alert(`Rack layout saved: ${result.rows} rows × ${result.cols} columns.`);
        window.location.reload();
      } catch (error) {
        errorBox.textContent = error instanceof Error ? error.message : "Could not save the rack layout.";
        errorBox.hidden = false;
        saveButton.disabled = false;
        saveButton.textContent = "Save layout";
      }
    });
    rowsInput.focus();
  }

  let lastGridSettingsAt = 0;
  let gridSettingsPromise = null;
  let cachedRackColumns = null;
  async function updateRackGridColumns() {
    if (!onCompartmentsPage()) return;
    const grid = root.querySelector(".rack-grid");
    if (!grid) return;
    if (cachedRackColumns !== null) {
      grid.classList.add("rack-layout-configured");
      grid.style.setProperty("--rack-columns", String(cachedRackColumns));
    }
    const now = Date.now();
    if (!gridSettingsPromise && now - lastGridSettingsAt > 5000) {
      lastGridSettingsAt = now;
      gridSettingsPromise = getJson("/api/compartments/layout")
        .then((layout) => {
          cachedRackColumns = Number(layout.cols) || 6;
          const currentGrid = root.querySelector(".rack-grid");
          if (currentGrid) {
            currentGrid.classList.add("rack-layout-configured");
            currentGrid.style.setProperty("--rack-columns", String(cachedRackColumns));
          }
        })
        .catch(() => {})
        .finally(() => { gridSettingsPromise = null; });
    }
  }

  function addRackLayoutButton() {
    const toolbar = root.querySelector(".page-content .page-stack .toolbar");
    if (!onCompartmentsPage() || !toolbar || toolbar.querySelector(".rack-layout-button")) return;
    const button = document.createElement("button");
    button.type = "button";
    button.className = "rack-layout-button";
    button.textContent = "Rack layout";
    button.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      void openRackLayoutDialog();
    });
    toolbar.appendChild(button);
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
      addRackLayoutButton();
      void updateRackGridColumns();
    });
  };

  new MutationObserver(refreshControls).observe(root, { childList: true, subtree: true });
  refreshControls();
})();

