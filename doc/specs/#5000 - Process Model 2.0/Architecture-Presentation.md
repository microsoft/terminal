---
marp: true
theme: default
Title: Terminal Process Model 2.0
paginate: true
footer: 'Terminal Process Model 2.0'
---

# Terminal Process Model 2.0

Mike Griese

---

# PROBLEM – Tab tear-out & merge

<div class="twocols">

### WINDOW RELIABILITY
   We’d rather that crashes in one window don’t end up bringing down all your Terminal windows, so we should have one top-level window per-process
### XAML ISLANDS
   The Terminal is a XAML Islands application (with no path to WinUI 3 at the moment). There’s no way to have multiple XI windows in the same process currently.

<p class="break"></p>

### BUFFER CONTENTS
   How do we move 9001x32 (or more) characters, attributes, and other properties from one process to another quickly & efficiently?
### PERFORMANCE
   Any solution cannot meaningfully impact overall Terminal throughput and input responsiveness

</div>

---

# SOLUTION

## WINDOW AND CONTENT PROCESSES

Separate the Terminal into two types of process – Window, and Content.

### Window
TODO!
### Content
TODO!

---



<style>
div.twocols {
  /* margin-top: 35px; */
  /* margin-bottom: 35px; */
  column-count: 2;
}
div.twocols p:first-child,
div.twocols img: { height: 70%} ,
div.twocols h1:first-child,
div.twocols h2:first-child,
div.twocols ul:first-child,
div.twocols ul li:first-child,
div.twocols ul li p:first-child {
  margin-top: 0 !important;
}
div.twocols p.break {
  break-before: column;
  margin-top: 0;
}
</style>


<div class="twocols">

<img src="./figure-001.png" height="600px">

<p class="break"></p>


# Current Process Model

* All one process
* `Connection` talks to ConPTY (conhost), which is responsible for communicating with client apps (`cmd`, `pwsh`, `WSL`, `ssh`)
* `TermControl` is the XAML control responsible for drawing the buffer to the screen, via DX
* `App` hosts the UI of the app, including other XAML elements

</div>

---

<div class="twocols">

<img src="./figure-004.png" height="500px" >

<p class="break"></p>

# Current Process Model

* All one process
* `App` hosts the UI of the app, including other XAML elements
* Multiple tabs each host their own `TermControl`s, who talk to their own `Connection`s

</div>

---

<div class="twocols">

<img src="./figure-002.png" height="500px" >

<p class="break"></p>

# Proposed Process Model

* Each connection / client / terminal instance lives in its own “Content Process”
* All XAML lives in a “Window Process”
* Windows host the content of multiple Content Processes

</div>

---

<div class="twocols">

<img src="./figure-003.png" height="500px" >

<p class="break"></p>

# Proposed Process Model

* Each connection / client / terminal instance lives in its own “Content Process”
* All XAML lives in a “Window Process”
* Windows host the content of multiple Content Processes

</div>

---

<div class="twocols">

<img src="./figure-005.png" height="500px" >

<p class="break"></p>

# Proposed Process Model

* Each connection / client / terminal instance lives in its own “Content Process”
* All XAML lives in a “Window Process”
* Windows host the content of multiple Content Processes

</div>

---

# PROOF OF CONCEPT

![](proof-of-concept-000.gif)

---

# Drop tab on an existing window

<img src="./drop-tab-on-existing-window.png" height="500px" >


---

# Questions


---

# Addenda

---

# Window-Content IPC

* We’d rather not roll our own IPC mechanism to communicate between Window and Content processes
* WinRT (COM) already gives us a rich set of features for defining methods, events
* WinRT can already be used x-proc
* Small refactors to the `TermControl` code enabled us to create a WinRT boundary between the XAML code and the “core”


---

# Serialization of a Tab?

* TODO!
* we used the existing serialization code, it was grand!

