const fs = require("fs");
const path = require("path");

const htmlPath = path.join(__dirname, "..", "server", "static", "index.html");
const html = fs.readFileSync(htmlPath, "utf8");
const blocks = [...html.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/gi)];

if (!blocks.length) {
  throw new Error("No se encontró JavaScript embebido en index.html");
}

for (const [, source] of blocks) {
  if (source.trim()) new Function(source);
}

console.log(`JavaScript válido: ${blocks.length} bloque(s) revisado(s).`);
