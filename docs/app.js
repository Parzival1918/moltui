document.addEventListener("DOMContentLoaded", () => {
    // Install tabs
    const installTabBtns = document.querySelectorAll("#install .tab-btn");
    const installPanels = document.querySelectorAll("#install .install-panel");

    installTabBtns.forEach((btn) => {
        btn.addEventListener("click", () => {
            const target = btn.dataset.tab;
            installTabBtns.forEach((b) => {
                const active = b === btn;
                b.classList.toggle("active", active);
                b.setAttribute("aria-selected", String(active));
            });
            installPanels.forEach((panel) => {
                panel.classList.toggle("active", panel.id === `tab-${target}`);
            });
        });
    });

    // Copy buttons
    document.querySelectorAll(".copy-btn").forEach((btn) => {
        btn.addEventListener("click", async () => {
            const text = btn.dataset.copy;
            if (!text) return;
            try {
                await navigator.clipboard.writeText(text);
                const prev = btn.textContent;
                btn.textContent = "Copied";
                setTimeout(() => {
                    btn.textContent = prev;
                }, 1400);
            } catch {
                btn.textContent = "Failed";
            }
        });
    });

    // Reference filter + category tabs
    let activeCat = "all";
    const refButtons = document.querySelectorAll("#reference .tab-btn");
    const refSearch = document.getElementById("ref-search");
    const refRows = document.querySelectorAll("#ref-tbody tr");

    const applyRefFilter = () => {
        const query = (refSearch?.value || "").trim().toLowerCase();
        refRows.forEach((row) => {
            const catMatch = activeCat === "all" || row.dataset.cat === activeCat;
            const textMatch = !query || row.textContent.toLowerCase().includes(query);
            row.style.display = catMatch && textMatch ? "" : "none";
        });
    };

    refButtons.forEach((btn) => {
        btn.addEventListener("click", () => {
            activeCat = btn.dataset.ref || "all";
            refButtons.forEach((b) => b.classList.toggle("active", b === btn));
            applyRefFilter();
        });
    });

    refSearch?.addEventListener("input", applyRefFilter);

    // Lightbox modal for gallery screenshots
    const lightbox = document.getElementById("lightbox");
    const lightboxImg = document.getElementById("lightbox-img");
    const lightboxCaption = document.getElementById("lightbox-caption");
    const lightboxClose = document.getElementById("lightbox-close");

    const closeLightbox = () => {
        lightbox?.classList.remove("open");
    };

    document.querySelectorAll(".shot-img-wrap").forEach((wrap) => {
        wrap.addEventListener("click", () => {
            const img = wrap.querySelector("img");
            if (!img || !lightbox || !lightboxImg || !lightboxCaption) return;
            lightboxImg.src = img.src;
            lightboxImg.alt = img.alt;
            lightboxCaption.textContent = wrap.dataset.caption || img.alt;
            lightbox.classList.add("open");
        });
    });

    lightboxClose?.addEventListener("click", closeLightbox);
    lightbox?.addEventListener("click", (e) => {
        if (e.target === lightbox) closeLightbox();
    });
    document.addEventListener("keydown", (e) => {
        if (e.key === "Escape") closeLightbox();
    });
});
