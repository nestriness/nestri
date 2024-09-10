export const relayDomain =
  {
    production: "fst.so",
    dev: "dev.fst.so",
  }[$app.stage] || $app.stage + ".dev.fst.so";