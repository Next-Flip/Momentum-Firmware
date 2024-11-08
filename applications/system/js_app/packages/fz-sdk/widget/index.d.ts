/**
 * Displays a customizable Widget on screen
 * @version Available with JS feature `widget`
 * @module
 */

type ComponentId = number;

/**
 * @brief Add a box component
 * @param x Horizontal position
 * @param y Vertical position
 * @param w Width
 * @param h Height
 */
export declare function addBox(x: number, y: number, w: number, h: number): ComponentId;

/**
 * @brief Add a circle component
 * @param x Horizontal position
 * @param y Vertical position
 * @param r Radius
 */
export declare function addCircle(x: number, y: number, r: number): ComponentId;

/**
 * @brief Add a disc component
 * @param x Horizontal position
 * @param y Vertical position
 * @param r Radius
 */
export declare function addDisc(x: number, y: number, r: number): ComponentId;

/**
 * @brief Add a dot component
 * @param x Horizontal position
 * @param y Vertical position
 */
export declare function addDot(x: number, y: number): ComponentId;

/**
 * @brief Add a frame component
 * @param x Horizontal position
 * @param y Vertical position
 * @param w Width
 * @param h Height
 */
export declare function addFrame(x: number, y: number, w: number, h: number): ComponentId;

/**
 * @brief Add a glyph component
 * @param x Horizontal position
 * @param y Vertical position
 * @param ch ASCII character code (eg. `"C".charCodeAt(0)`)
 */
export declare function addGlyph(x: number, y: number, ch: number): ComponentId;

/**
 * @brief Add an icon component
 * @param x Horizontal position
 * @param y Vertical position
 * @param icon Name of the icon (eg. `"ButtonUp_7x4"`)
 * @version Available with JS feature `widget-addicon`
 */
export declare function addIcon(x: number, y: number, icon: string): ComponentId;

/**
 * @brief Add a line component
 * @param x1 Horizontal position 1
 * @param y1 Vertical position 1
 * @param x2 Horizontal position 2
 * @param y2 Vertical position 2
 */
export declare function addLine(x1: number, y1: number, x2: number, y2: number): ComponentId;

/**
 * @brief Add a rounded box component
 * @param x Horizontal position
 * @param y Vertical position
 * @param w Width
 * @param h Height
 * @param r Radius
 */
export declare function addRbox(x: number, y: number, w: number, h: number, r: number): ComponentId;

/**
 * @brief Add a rounded frame component
 * @param x Horizontal position
 * @param y Vertical position
 * @param w Width
 * @param h Height
 * @param r Radius
 */
export declare function addRframe(x: number, y: number, w: number, h: number, r: number): ComponentId;

/**
 * @brief Add a text component
 * @param x Horizontal position
 * @param y Vertical position
 * @param font What font to use, Primary or Secondary
 * @param text Text to display
 */
export declare function addText(x: number, y: number, font: "Primary" | "Secondary", text: string): ComponentId;

type XbmId = number;

/**
 * @brief Add an xbm image component
 * @param x Horizontal position
 * @param y Vertical position
 * @param index Loaded xbm id to use
 */
export declare function addXbm(x: number, y: number, index: XbmId): ComponentId;

/**
 * @brief Load an xbm image sprite
 * @param path Xbm file to load
 */
export declare function loadImageXbm(path: string): XbmId;

/**
 * @brief Remove a component
 * @param id Component id to remove
 */
export declare function remove(id: ComponentId): boolean;

/**
 * @brief Check if the widget view is shown
 */
export declare function isOpen(): boolean;

/**
 * @brief Show the widget view
 */
export declare function show(): void;

/**
 * @brief Close the widget view
 */
export declare function close(): void;
