Here is a list of needed/wanted features.
The most urgent ones, to make this a functioning tool, are marked with `(mvp1)` or `(mvp2)`

# Packer

* [ ] General: Unify identical sprite images in atlas image
* [ ] General: Set default image trim modes (Polygon, Rect)
* [ ] TilePacker: Support tesselating concave geometry (e.g. with holes)

# Sprite editor:

* [ ] `(mvp2)` Add a sprite editor view/tab
* [ ] `(mvp2)` Edit sprite pivot point (absolute + uniform (0,0) is center)
* [ ] Set trim mode (e.g. Polygon, Rect, None)
* [ ] Set rotate override mode (e.g. None, if it mustn't be )

# Atlas view

* [ ] Show sprite pivot point in atlas view (add option to turn it off)
* [ ] Show stats:
    * packing occupancy %
    * # vertices
    * Time it took to pack
* [ ] `(mvp1)` Add shortcut legend for zooming/panning with the mouse (CMD+swipe up/down to zoom, swipe to move around)
* [ ] Hover/Clicking item(s) should select them in the outline


# Exporter

* [ ] `(mvp1)` Add image output options for png, tga etc
* [ ] Add png post process step (e.g. TinyPng?)
* [ ] Add more default exporters: .json, etc

# Outline

* [ ] `(mvp2)` Double click item -> Open Image in sprite editor
* [ ] `(mvp2)` Press "F" key to zoom in and "frame" the current selected items
* [ ] Add a filter field, to help finding items in a massive list
* [ ] Shift+Left click to increase selection
* [ ] Clicking on the columns heads should sort the items.

App:

* [ ] `(mvp1)` Implement a "dirty" flag (and check when closing the project)
* [ ] `(mvp1)` Documentation
* [ ] `(mvp1)` Windows support
* [ ] Linux support
* [ ] `(mvp1)` App Icon - macOS/Windows
* [ ] About dialog with version number, and info
* [ ] Outline/App: Add support for drag&drop of folder/file
* [ ] Tooltips on hover (for most UI elements)
* [ ] Improve the menu bar: https://github.com/thomashope/native-menu-bar

# Robustness

* [ ] More testing: Make sure no invalid configurations crash the tool

Releases:

* [ ] Release: Bundle the app into a zip, with the exporters
* [ ] Release: CI: When a release is created, make sure to run an action that compiles and adds prebuilt artifacts to the release.

