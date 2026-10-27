# 给组件加样式

`fb-*` 系列 Web Components 本身不带样式，未定义样式时仅保留基础占位。各组件的参考文档详细列出了所支持的 CSS Part、插槽（Slot）与宿主属性（Host Attributes）；本页阐述如何组合运用这三种定制机制。设计理念见[组件的工作方式](/zh/concepts/components)。

## 使用 `::part()` 定制内部元素样式 {#用-part-给内部元素加样式}

在样式表里选中元素的某个 part：

```css
/* 圆形的播放按钮 */
fb-play-button::part(button) {
    width: 48px;
    height: 48px;
    border: none;
    border-radius: 50%;
    background: #1db954;
    color: white;
    cursor: pointer;
}
fb-play-button::part(button):hover {
    background: #1ed760;
}

/* 进度条有轨道、填充段和滑块三个 part */
fb-seek-bar::part(track) {
    background: linear-gradient(#333, #333) center / 100% 4px no-repeat;
}
fb-seek-bar::part(fill) { height: 4px; background: #1db954; }
fb-seek-bar::part(thumb) { width: 12px; height: 12px; border-radius: 50%; background: #fff; }
```

`::part()` 仅能命中 Shadow DOM 中显式声明了 `part` 属性的目标元素，无法直接穿透选择其深层后代。样式规则需直接作用于该 Part 元素自身。

## 利用宿主属性响应状态变更 {#用宿主属性响应状态}

组件将内部状态同步反映为宿主元素属性，可与 `::part()` 组合使用：

```css
/* 播放中时突出播放按钮 */
fb-play-button[playing]::part(button) {
    background: #ffffff;
    color: #1db954;
}

/* 静音时把音量控件调暗 */
fb-volume-control[muted] {
    opacity: 0.5;
}
```

`::part()` 既可与宿主状态属性组合（如前文示例），亦可配合 `:hover`、`:focus-visible` 等用户交互伪类。注意：`::part()` 之后追加属性选择器（如 `::part(btn)[active]`）不符合标准 CSS 语法；组件若在内部子元素上标注私有属性（例如 `<fb-rating>` 内部星标的 `data-filled`），外部样式表无法直接选取，应通过宿主属性或组件提供的 Part 进行定制。

## 通过插槽（Slot）替换默认内容 {#用-slot-替换内容}

在具名 slot 中填入自定义标记，以替换默认图标或控件：

```html
<fb-play-button>
    <svg slot="play-icon" viewBox="0 0 24 24" width="20" height="20"><path d="M8 5v14l11-7z" /></svg>
    <svg slot="pause-icon" viewBox="0 0 24 24" width="20" height="20"><path d="M6 5h4v14H6zm8 0h4v14h-4z" /></svg>
</fb-play-button>
```

组件依据运行时状态自主控制显示哪个 slot（此处为播放时呈现暂停图标）。

## 统筹管理主题样式 {#把样式放在一处}

Web Components 样式属于标准 CSS，通常与应用的其他视图样式集中存放在主题样式表中。在 `:root` 上定义的 CSS 变量（例如主题色与设计令牌）可自然级联渗透至组件内部。[构建第一个主题](/zh/tutorials/first-theme)演示了完整的播放器样式编写方式。
