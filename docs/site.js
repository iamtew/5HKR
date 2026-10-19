const links = [...document.querySelectorAll("nav a")];
const sections = links
  .map((a) => document.querySelector(a.getAttribute("href")))
  .filter(Boolean);

const watch = new IntersectionObserver((entries) => {
  entries.forEach((entry) => {
    if (!entry.isIntersecting) return;
    const id = "#" + entry.target.id;
    links.forEach((a) => a.classList.toggle("on", a.getAttribute("href") === id));
  });
}, { rootMargin: "-40% 0px -50% 0px" });

sections.forEach((section) => watch.observe(section));
if (document.body.dataset.note !== undefined)
  document.querySelector('nav a[href$="#notes"]')?.classList.add("on");
