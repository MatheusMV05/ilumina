import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import { App } from "./App";
import { Intro } from "./components/Intro";
import { Sky } from "./components/Sky";
import "./styles.css";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <Sky />
    <App />
    <Intro />
  </StrictMode>,
);
