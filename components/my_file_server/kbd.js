// 配置元数据
const KEY_CONFIG_SCHEMA = {
    ioTypeNames: {
        0: "普通按键",
        1: "矩阵按键",
        2: "编码器按键"
    },
    ioTypeLayoutNames: {
        0: "normalKeys",
        1: "matrixKeys",
        2: "encoders"
    },
    layerNames: {
        0: "原始层",
        1: "fn层",
        2: "fn2层"
    },
    keyTypes: {
        0: { name: "不设置", hasValue: false },
        1: { name: "hid按键码", hasValue: true, valueType: "keycode" },
        2: { name: "ascii字符码", hasValue: true, valueType: "asciiCode" },
        3: { name: "多媒体按键码", hasValue: true, valueType: "consumerCode" },
        4: { name: "空按键", hasValue: false },
        5: { name: "fn键", hasValue: false },
        6: { name: "fn2键", hasValue: false },
        7: { name: "切换fn键", hasValue: false },
        8: { name: "设备控制按键码", hasValue: true, valueType: "devControlCode" }
    },

    valueLabels: {
        keycode: {
            0: "NONE",
            1: "NONE",
            2: "NONE",
            3: "NONE",
            4: "a/A",
            5: "b/B",
            6: "c/C",
            7: "d/D",
            8: "e/E",
            9: "f/F",
            10: "g/G",
            11: "h/H",
            12: "i/I",
            13: "j/J",
            14: "k/K",
            15: "l/L",
            16: "m/M",
            17: "n/N",
            18: "o/O",
            19: "p/P",
            20: "q/Q",
            21: "r/R",
            22: "s/S",
            23: "t/T",
            24: "u/U",
            25: "v/V",
            26: "w/W",
            27: "x/X",
            28: "y/Y",
            29: "z/Z",
            30: "1/!",
            31: "2/@",
            32: "3/#",
            33: "4/$",
            34: "5/%",
            35: "6/^",
            36: "7/&",
            37: "8/*",
            38: "9/(",
            39: "0/)",
            40: "回车/Enter",
            41: "ESC",
            42: "退格/Backspace",
            43: "TAB",
            44: "空格/Space",
            45: "-/_",
            46: "=/+",
            47: "[/{",
            48: "]/}",
            49: "\\/|",
            50: "EUROPE_1",
            51: ";/:",
            52: "'/\"",
            53: "`/~",
            54: ",/<",
            55: "./>",
            56: "//?",
            57: "CapsLock",
            58: "F1",
            59: "F2",
            60: "F3",
            61: "F4",
            62: "F5",
            63: "F6",
            64: "F7",
            65: "F8",
            66: "F9",
            67: "F10",
            68: "F11",
            69: "F12",
            70: "Print Screen",
            71: "Scroll Lock",
            72: "Pause",
            73: "Insert",
            74: "Home",
            75: "Page Up",
            76: "Delete",
            77: "End",
            78: "Page Down",
            79: "方向键右",
            80: "方向键左",
            81: "方向键下",
            82: "方向键上",
            83: "NumLock",
            84: "小键盘/",
            85: "小键盘*",
            86: "小键盘-",
            87: "小键盘+",
            88: "小键盘回车",
            89: "小键盘1",
            90: "小键盘2",
            91: "小键盘3",
            92: "小键盘4",
            93: "小键盘5",
            94: "小键盘6",
            95: "小键盘7",
            96: "小键盘8",
            97: "小键盘9",
            98: "小键盘0",
            99: "小键盘.",
            100: "EUROPE_2",
            101: "APPLICATION",
            102: "POWER",
            103: "KEYPAD_EQUAL",
            104: "F13",
            105: "F14",
            106: "F15",
            107: "F16",
            108: "F17",
            109: "F18",
            110: "F19",
            111: "F20",
            112: "F21",
            113: "F22",
            114: "F23",
            115: "F24",
            116: "EXECUTE",
            117: "HELP",
            118: "MENU",
            119: "SELECT",
            120: "STOP",
            121: "AGAIN",
            122: "UNDO",
            123: "CUT",
            124: "COPY",
            125: "PASTE",
            126: "FIND",
            127: "MUTE",
            128: "VOLUME_UP",
            129: "VOLUME_DOWN",
            130: "LOCKING_CAPS_LOCK",
            131: "LOCKING_NUM_LOCK",
            132: "LOCKING_SCROLL_LOCK",
            133: "KEYPAD_COMMA",
            134: "KEYPAD_EQUAL_SIGN",
            135: "KANJI1",
            224: "左Ctrl",
            225: "左Shift",
            226: "左Alt",
            227: "GUI_LEFT",
            228: "右Ctrl",
            229: "右Shift",
            230: "右Alt",
            231: "GUI_RIGHT"
        },
        asciiCode: {
            8: "退格/Backspace",
            9: "Tab",
            10: "回车/Enter",
            13: "回车/Enter",
            27: "ESC",
            32: "空格/Space",
            33: "!",
            34: "\"",
            35: "#",
            36: "$",
            37: "%",
            38: "&",
            39: "'",
            40: "(",
            41: ")",
            42: "*",
            43: "+",
            44: ",",
            45: "-",
            46: ".",
            47: "/",
            48: "0",
            49: "1",
            50: "2",
            51: "3",
            52: "4",
            53: "5",
            54: "6",
            55: "7",
            56: "8",
            57: "9",
            58: ":",
            59: ";",
            60: "<",
            61: "=",
            62: ">",
            63: "?",
            64: "@",
            65: "A",
            66: "B",
            67: "C",
            68: "D",
            69: "E",
            70: "F",
            71: "G",
            72: "H",
            73: "I",
            74: "J",
            75: "K",
            76: "L",
            77: "M",
            78: "N",
            79: "O",
            80: "P",
            81: "Q",
            82: "R",
            83: "S",
            85: "T",
            85: "U",
            86: "V",
            87: "W",
            88: "X",
            89: "Y",
            90: "Z",
            91: "[",
            92: "\\",
            93: "]",
            94: "^",
            95: "_",
            96: "`",
            97: "a",
            98: "b",
            99: "c",
            102: "d",
            101: "e",
            102: "f",
            103: "g",
            104: "h",
            105: "i",
            106: "j",
            107: "k",
            108: "l",
            109: "m",
            110: "n",
            111: "o",
            112: "p",
            113: "q",
            114: "r",
            115: "s",
            117: "t",
            117: "u",
            118: "v",
            119: "w",
            120: "x",
            121: "y",
            122: "z",
            123: "{",
            124: "|",
            125: "}",
            126: "~",
            127: "Delete"
        },
        consumerCode: {
            181: "下一曲",
            182: "上一曲",
            183: "停止",
            205: "播放/暂停",
            224: "音量",
            226: "静音",
            227: "低音",
            228: "高音",
            229: "低音增强",
            233: "音量+",
            234: "音量-",
            338: "低音+",
            339: "低音-",
            340: "高音+",
            341: "高音-",
        },
        devControlCode: {
            0: "重启设备",
            1: "进入深睡",
            2: "进入MSC模式",
            3: "开关USB键盘",
            4: "开关蓝牙键盘",
            5: "打开STA功能",
            6: "打开AP功能",
            7: "打开ESPNOW",
            8: "开关屏幕",
            9: "切换屏幕亮度",
            10: "开关深睡检测",
            11: "配对ESPNOW",
            12: "切换LOG等级",
            13: "清空NVS数据",
            14: "切换屏幕桌面",
            15: "切换led灯模式",
            16: "切换led灯亮度",
        }
    }
};

// 物理布局配置
const PHYSICAL_LAYOUT = {
    width: 80,
    height: 80,

    normalKeys: {
        number: 0,
        ioType: 0,
        positions: [
            { id: 0, x: 0, y: 0, width: 1, height: 1 },
            { id: 1, x: 1.25, y: 0, width: 1, height: 1 },
            { id: 2, x: 2.5, y: 0, width: 1, height: 1 },
            { id: 3, x: 3.75, y: 0, width: 1, height: 1 }
        ],
        groupLabel: "普通按键区"
    },

    matrixKeys: {
        ioType: 1,
        rows: 6,
        cols: 5,
        startPos: { x: 0, y: 0 },
        spacing: 1,// 两个按键原点之间的单位距离，如果要不重叠，应该大于等于1
        hideKeys: {
            index: [],
            vector: [
                { row: 0, col: 4 },
                { row: 1, col: 4 },
                { row: 2, col: 4 },
                { row: 3, col: 4 },
                { row: 4, col: 4 }
            ]
        },
        groupLabel: "矩阵按键区"
    },

    encoders:
    {
        ioType: 2,
        number: 2,
        startPos: { x: 5, y: 0 },
        positions: [
            { id: 0, x: 0, y: 0, width: 1, height: 1 },
            { id: 1, x: 1, y: 0, width: 1, height: 1 }
        ]
    },
    groupLabel: "编码器按键区"
};

// API端点
const API_KBD_CONFIG = '/api/dev/kbdconfig'; // 获取键盘按键配置
const API_KBD_SET_CONFIG = '/api/dev/setconfig?type=kbd';
const API_DEV_STATUS = '/api/dev';
const REQUEST_TIMEOUT = 30000;

// 全局变量
let defaultKbdConfig = {};

let currentKbdConfig = {};
let currentHost = window.location.hostname;
let activeKeyElement = null;
let activeConfigPanel = null;
let pendingRequest = null;

// DOM元素
const configContainer = document.getElementById('config-container');
const saveBtn = document.getElementById('save-btn');
const resetBtn = document.getElementById('reset-btn');
const fetchConfigBtn = document.getElementById('fetch-config-btn');
const checkStatusBtn = document.getElementById('check-status-btn');
const hostInput = document.getElementById('host-input');
const statusIndicator = document.getElementById('status-indicator');
const statusText = document.getElementById('status-text');

// 初始化
document.addEventListener('DOMContentLoaded', init);

function init() {
    hostInput.value = currentHost;
    setupEventListeners();
    createDatalist();
    checkServerStatus();
    loadKbdConfig();
}

function createDatalist() {
    Object.entries(KEY_CONFIG_SCHEMA.keyTypes).map(([_, keyInfo]) => {
        if (!keyInfo.hasValue) return '';
        const valueList = KEY_CONFIG_SCHEMA.valueLabels[keyInfo.valueType];
        if (!valueList) return '';
        let datalist = document.createElement('datalist');
        datalist.id = `${keyInfo.valueType}`;
        Object.entries(valueList).map(([key, label]) => {
            datalist.innerHTML += `<option value="${key}">${label}</option>`;
        });
        document.body.appendChild(datalist);
    })
}

function setupEventListeners() {
    checkStatusBtn.addEventListener('click', checkServerStatus);
    saveBtn.addEventListener('click', saveKbdConfig);
    resetBtn.addEventListener('click', resetKbdConfig);
    fetchConfigBtn.addEventListener('click', fetchLatestConfig);
    hostInput.addEventListener('change', updateHost);

    // 点击外部关闭配置面板
    document.addEventListener('click', closeConfigPanel);
}

function updateHost(e) {
    currentHost = e.target.value.trim() || window.location.hostname;
}

function setButtonLoading(button, isLoading) {
    button.disabled = isLoading;
    if (isLoading) {
        button.innerHTML = `<span class="spinner"></span> ${button.textContent}`;
    } else {
        button.textContent = button.dataset.originalText || button.textContent;
    }
}

// 服务器状态检查
async function checkServerStatus() {
    if (pendingRequest) return;

    try {
        setButtonLoading(checkStatusBtn, true);
        pendingRequest = fetchWithTimeout(API_DEV_STATUS, { timeout: REQUEST_TIMEOUT });
        await pendingRequest;

        setServerOnline(true);
    } catch (error) {
        setServerOnline(false);
        console.error("服务器状态检查失败:", error);
        showError("服务器状态检查失败: " + error.message);
    } finally {
        pendingRequest = null;
        setButtonLoading(checkStatusBtn, false);
    }
}

function setServerOnline(online) {
    statusIndicator.className = online ? 'online' : 'offline';
    statusText.textContent = online ? '在线' : '离线';
    updateUIState();
}

// 键盘配置加载
async function loadKbdConfig() {
    try {
        const response = await fetchWithTimeout(getApiUrl(API_KBD_CONFIG));
        if (!response.ok) throw new Error(`HTTP错误: ${response.status}`);

        const config = await response.json();
        defaultKbdConfig = JSON.parse(JSON.stringify(config));
        currentKbdConfig = JSON.parse(JSON.stringify(config));

        renderKeyboard();
        updateUIState();
    } catch (error) {
        console.error("加载键盘配置失败:", error);
        showError("加载配置失败: " + error.message);
    }
}

// 将原本没有按层分类的配置按层分类
function groupConfigByLayer() {
    const layers = [];
    currentKbdConfig.keyConfigsArr.forEach(
        config => {
            if (!layers[config.layer]) {
                layers[config.layer] = []
            };
            layers[config.layer].push(config);
        }
    );
    return layers;
}

// 渲染键盘布局
function renderKeyboard() {
    const layers = groupConfigByLayer();

    const html = `
        <div class="kbd-config-container">
            <div class="layer-tabs">
                ${layers.map((_, index) => `
                    <button class="layer-tab ${index === 0 ? 'active' : ''}" 
                            data-layer="${index}">
                       ${KEY_CONFIG_SCHEMA.layerNames[index].trim() || `第${index}层`}
                    </button>
                `).join('')}
            </div>
            
            ${layers.map((layerConfigs, layerIndex) => `
                <div class="keyboard-layer" data-layer="${layerIndex}" 
                     style="${layerIndex === 0 ? '' : 'display: none;'}">
                    ${renderPhysicalLayout(layerConfigs)}
                </div>
            `).join('')}
        </div>
    `;

    configContainer.innerHTML = html;

    // 绑定事件
    bindLayerTabEvents();
    bindKeyEvents();
}

// 传入某一层的3种按键配置
function renderPhysicalLayout(layerConfigs) {

    let normalKeysHtml = '';
    let matrixKeysHtml = '';
    let encoderKeysHtml = '';

    let normalKeysConfig = [];
    let matrixKeysConfig = [];
    let encoderKeysConfig = [];

    layerConfigs.forEach(config => {
        if (config.ioType === 0) {
            normalKeysConfig.push(config);
        } else if (config.ioType === 1) {
            matrixKeysConfig.push(config);
        } else if (config.ioType === 2) {
            encoderKeysConfig.push(config);
        }
    });

    if (PHYSICAL_LAYOUT.normalKeys.number > 0) {
        normalKeysHtml = `
            <!-- 普通按键 -->
            <div class="key-group normal-keys" style="left: 10px; top: 10px; width: 0px; height: 0px;">
                ${renderNormalKeys(normalKeysConfig)}
            </div>
        `
    }

    if (PHYSICAL_LAYOUT.matrixKeys.cols * PHYSICAL_LAYOUT.matrixKeys.rows > 0) {
        matrixKeysHtml = `
            <!-- 矩阵按键 -->
            <div class="key-group matrix-keys" style="left: 10px; top: 10px;width: 0px; height: 0px;">
                ${renderMatrixGrid(matrixKeysConfig)}
            </div>
        `
    }

    if (PHYSICAL_LAYOUT.encoders.number > 0) {
        encoderKeysHtml = `
            <!-- 编码器 -->
            <div class="key-group encoder-keys" style="left: 10px; top: 10px;width: 0px; height: 0px;">
            ${renderEncoders(encoderKeysConfig)}
            </div>
        `
    }

    return `
        <div class="keyboard-physical">
            ${normalKeysHtml}

            ${matrixKeysHtml}

            ${encoderKeysHtml}
        </div>
    `;
}

function renderNormalKeys(layerConfigs) {
    const { ioType, number, positions } = PHYSICAL_LAYOUT.normalKeys;
    let html = '';
    let n = number > positions.length ? positions.length : number;
    for (let i = 0; i < n; i++) {
        let { id, x, y, width, height } = positions[i];
        const keyConfig = findKeyConfig(layerConfigs, id);
        if (!keyConfig) continue;
        x = (x) * PHYSICAL_LAYOUT.width;
        y = (y) * PHYSICAL_LAYOUT.height;

        html += `
                <div class="phys-key" 
                     style="left: ${x}px; top: ${y}px;
                             width: ${width * PHYSICAL_LAYOUT.width}px; height: ${height * PHYSICAL_LAYOUT.height}px;"
                     data-id="${id}" data-layer="${layerConfigs[0].layer}" data-ioType="${ioType}">
                    ${renderKeyDisplay(keyConfig)}
                </div>
            `;
    }
    return html;

    // return PHYSICAL_LAYOUT.normalKeys.positions.map(
    //     pos => {
    //         const keyConfig = findKeyConfig(layerConfigs, pos.id);
    //         if (!keyConfig) return '';

    //         return `
    //         <div class="phys-key" 
    //              style="left: ${pos.x * PHYSICAL_LAYOUT.width}px; top: ${pos.y * PHYSICAL_LAYOUT.height}px;
    //                     width: ${pos.width * PHYSICAL_LAYOUT.width}px; height: ${pos.height * PHYSICAL_LAYOUT.height}px;"
    //              data-id="${pos.id}" data-layer="${layerConfigs[0].layer}" data-ioType="${layerConfigs[0].ioType}">
    //             <div class="key-id">键 ${pos.id}</div>
    //             ${renderKeyDisplay(keyConfig)}
    //         </div>
    //     `;
    //     }
    // ).join('');
}

function renderMatrixGrid(layerConfigs) {
    const { ioType, rows, cols, startPos, spacing, hideKeys } = PHYSICAL_LAYOUT.matrixKeys;
    let html = '';
    hideKeys.vector.forEach(v => {
        const id = v.row * cols + v.col;
        if (!hideKeys.index.includes(id)) {
            hideKeys.index.push(id);
        }
    })

    for (let row = 0; row < rows; row++) {
        for (let col = 0; col < cols; col++) {
            const id = row * cols + col;
            if (hideKeys.index.includes(id)) continue;
            let x = (startPos.x + col * spacing) * PHYSICAL_LAYOUT.width;
            let y = (startPos.y + row * spacing) * PHYSICAL_LAYOUT.height;
            // 第1行以下的按键向下偏移0.25个单位
            if (row > 0) y += 0.25 * PHYSICAL_LAYOUT.height;
            const keyConfig = findKeyConfig(layerConfigs, id);
            if (!keyConfig) continue;
            // 矩阵按键如果要单独设置按键大小，需要额外配置，这里先不处理，默认为1单位大小
            html += `
                <div class="matrix-key" 
                     style="left: ${x}px; top: ${y}px;
                     width: ${1 * PHYSICAL_LAYOUT.width}px; height: ${1 * PHYSICAL_LAYOUT.height}px;" 
                     data-id="${id}" data-layer="${layerConfigs[0].layer}" data-ioType="${ioType}">
                    ${renderKeyDisplay(keyConfig)}
                </div>
            `;
        }
    }

    return html;
}

function renderEncoders(layerConfigs) {
    const { ioType, number, startPos, positions } = PHYSICAL_LAYOUT.encoders;
    let html = '';
    let n = number > positions.length ? positions.length : number;
    for (let i = 0; i < n; i++) {

        let { id, x, y, width, height } = positions[i];
        const keyConfig = findKeyConfig(layerConfigs, id);
        if (!keyConfig) continue;
        x = (x + startPos.x) * PHYSICAL_LAYOUT.width;
        y = (y + startPos.y) * PHYSICAL_LAYOUT.height;

        html += `
                <div class="encoder-key" 
                     style="left: ${x}px; top: ${y}px;
                     width: ${width * PHYSICAL_LAYOUT.width}px; height: ${height * PHYSICAL_LAYOUT.height}px;"
                     data-id="${id}" data-layer="${layerConfigs[0].layer}" data-ioType="${ioType}">
                    ${renderKeyDisplay(keyConfig)}
                </div>
            `;
    }
    return html;

    // return PHYSICAL_LAYOUT.encoders.map(encoder => {
    //     const [cwId, ccwId] = encoder.idPair;
    //     const cwConfig = findKeyConfig(layerConfigs, cwId);
    //     const ccwConfig = findKeyConfig(layerConfigs, ccwId);

    //     return `
    //         <div class="encoder-group" 
    //              style="left: ${encoder.position.x * 60}px; top: ${encoder.position.y * 60}px;">
    //             <div class="encoder-label">${encoder.label}</div>
    //             <div class="encoder-pair" style="gap: ${encoder.spacing * 60}px">
    //                 <div class="encoder-key cw" 
    //                      data-id="${cwId}" data-layer="${layerConfigs[0].layer}">
    //                     <div class="encoder-arrow"></div>
    //                     ${renderKeyDisplay(cwConfig)}
    //                 </div>
    //                 <div class="encoder-key ccw" 
    //                      data-id="${ccwId}" data-layer="${layerConfigs[0].layer}">
    //                     <div class="encoder-arrow"></div>
    //                     ${renderKeyDisplay(ccwConfig)}
    //                 </div>
    //             </div>
    //         </div>
    //     `;
    // }).join('');
}

function renderKeyDisplay(keyConfig) {
    // return `<div class="key-value">${getValueDisplay(keyConfig)}</div>`;// 和下面的等效

    if (!keyConfig) return '<div class="key-value">未配置</div>';

    // 尝试匹配按键类型
    const typeInfo = KEY_CONFIG_SCHEMA.keyTypes[keyConfig.type];
    // 没有匹配的按键类型
    if (!typeInfo)
        return `<div class="key-value">type:${keyConfig.type}\n${keyConfig.value !== undefined ? `value:${keyConfig.value}` : ''}</div>`;

    // 有匹配的按键类型，获取类型名
    let displayText = typeInfo.name;
    // 如果类型为带值类型，则应该显示值名
    if (typeInfo.hasValue) {
        // 如果传入配置中没有值，说明有错误
        if (keyConfig.value === undefined) {
            displayText = `${typeInfo.name}:valueError`;
        }
        // 传入配置中确定有值 
        else {
            // 尝试获取值名的集合
            let keyNames = KEY_CONFIG_SCHEMA.valueLabels[typeInfo.valueType];
            // 如果不存在值名集合，使用类型名:值作为显示文本，理论上不该存在这种情况 
            if (!keyNames) {
                displayText = `${typeInfo.name}:${keyConfig.value}`;
            }
            // 如果存在值名集合
            else {
                let keyName = keyNames[keyConfig.value];
                displayText = keyName ? keyName : `${typeInfo.name}:${keyConfig.value}`;
            }
        }
    } else {
        // 如果这个按键类型不需要值，不需要进一步处理
    }

    return `<div class="key-value">${displayText}</div>`;
}

// 事件绑定
function bindLayerTabEvents() {
    document.querySelectorAll('.layer-tab').forEach(tab => {
        tab.addEventListener('click', (e) => {
            const layer = e.target.dataset.layer;

            // 更新标签状态
            document.querySelectorAll('.layer-tab').forEach(t => {
                t.classList.toggle('active', t === e.target);
            });

            // 更新层显示
            document.querySelectorAll('.keyboard-layer').forEach(layerEl => {
                layerEl.style.display = layerEl.dataset.layer === layer ? '' : 'none';
            });
        });
    });
}

function bindKeyEvents() {
    document.querySelectorAll('.phys-key, .matrix-key, .encoder-key').forEach(keyEl => {
        keyEl.addEventListener('click', (e) => {
            e.stopPropagation();
            openConfigPanel(keyEl);
        });
    });
}

function openConfigPanel(keyEl) {
    closeConfigPanel();

    const keyId = parseInt(keyEl.dataset.id);
    const ioType = parseInt(keyEl.dataset.iotype);
    const layer = parseInt(keyEl.dataset.layer);
    const keyConfig = findKeyConfig(currentKbdConfig.keyConfigsArr, keyId, layer, ioType);

    activeKeyElement = keyEl;
    keyEl.classList.add('active');

    const panelHtml = `
        <div class="key-config-panel">
            <h5>${KEY_CONFIG_SCHEMA.ioTypeNames[ioType]}:${keyId} (${KEY_CONFIG_SCHEMA.layerNames[layer]})</h5>
            <div class="config-input-group">
                <label>按键类型</label>
                <select id="type_${ioType}_${layer}_${keyId}" name="type_${ioType}_${layer}_${keyId}" class="key-type-select" data-key-id="${keyId}">
                    ${Object.entries(KEY_CONFIG_SCHEMA.keyTypes).map(([type, info]) => `
                        <option value="${type}" ${keyConfig?.type == type ? 'selected' : ''}>
                            ${info.name}
                        </option>
                    `).join('')}
                </select>
            </div>
            
            ${keyConfig && KEY_CONFIG_SCHEMA.keyTypes[keyConfig.type]?.hasValue ? `
                <div class="config-input-group">
                    <label>按键值</label>
                    <div class="value-selector">
                        <input  type="text" 
                                list = "${KEY_CONFIG_SCHEMA.keyTypes[keyConfig.type].valueType}"
                                class="value-search" 
                                id="ivalue_${ioType}_${layer}_${keyId}" name="ivalue_${ioType}_${layer}_${keyId}"
                                placeholder="输入值或通过下拉框选择..." 
                                value="${keyConfig.value}"
                                data-key-id="${keyId}">
                    </div>
                </div>
            ` : ''}
        </div>
    `;

    const panel = document.createElement('div');
    panel.innerHTML = panelHtml;
    keyEl.appendChild(panel);
    activeConfigPanel = panel;

    // 绑定面板事件
    bindPanelEvents(panel, keyConfig);
}

function bindPanelEvents(panel, originalConfig) {
    const typeSelect = panel.querySelector('.key-type-select');
    const valueSearch = panel.querySelector('.value-search');
    
    // 阻止面板内部元素的点击事件冒泡
    panel.querySelectorAll('select, input, option').forEach(element => {
        element.addEventListener('click', (e) => {
            e.stopPropagation();
        });
    });

    if (typeSelect) {
        // 按键类型改变时触发的事件
        typeSelect.addEventListener('change', (e) => {
            updateKeyType(parseInt(e.target.value));
        });

        typeSelect.addEventListener('click', (e) => {
            e.stopPropagation();
        });
    }

    if (valueSearch) {
        // 按键值改变时触发的事件
        valueSearch.addEventListener('change', (e) => {
            updateKeyValue(parseInt(e.target.value));
        });

        // 选项点击事件
        valueSearch.addEventListener('click', (e) => {
            e.stopPropagation();
        });
    }

    // 保存原始配置用于取消
    panel.dataset.originalConfig = JSON.stringify(originalConfig);
}

// 更新当前按键配置里的按键类型
function updateKeyType(newType, newValue = null) {
    const keyId = parseInt(activeKeyElement.dataset.id);
    const layer = parseInt(activeKeyElement.dataset.layer);
    const ioType = parseInt(activeKeyElement.dataset.iotype);

    let keyConfig = findKeyConfig(currentKbdConfig.keyConfigsArr, keyId, layer, ioType);
    if (!keyConfig) {
        // 创建新配置
        keyConfig = { id: keyId, type: newType };
        if (newValue) {
            keyConfig.value = newValue;
        }
        let layerConfig = currentKbdConfig.keyConfigsArr.find(
            l => l.layer === layer && l.ioType === ioType
        );
        if (!layerConfig) {
            layerConfig = { layer: layer, ioType: ioType, keysArr: [] };
            currentKbdConfig.keyConfigsArr.push(layerConfig);
        }
        layerConfig.keysArr.push(keyConfig);
    } else {
        keyConfig.type = newType;
        if (newValue) {
            keyConfig.value = newValue;
        }
    }

    // 重新渲染面板
    openConfigPanel(activeKeyElement);
    updateKeyDisplay();
    updateUIState();
}

function updateKeyValue(newValue) {
    const keyId = parseInt(activeKeyElement.dataset.id);
    const layer = parseInt(activeKeyElement.dataset.layer);
    const ioType = parseInt(activeKeyElement.dataset.iotype);
    const keyConfig = findKeyConfig(currentKbdConfig.keyConfigsArr, keyId, layer, ioType);

    if (keyConfig) {
        keyConfig.value = newValue;
        updateKeyDisplay();
        updateUIState();
    }
}

function updateKeyDisplay() {
    const keyId = parseInt(activeKeyElement.dataset.id);
    const layer = parseInt(activeKeyElement.dataset.layer);
    const ioType = parseInt(activeKeyElement.dataset.iotype);
    const keyConfig = findKeyConfig(currentKbdConfig.keyConfigsArr, keyId, layer, ioType);

    const displayEl = activeKeyElement.querySelector('.key-value');
    if (displayEl) {
        displayEl.textContent = getValueDisplay(keyConfig);
    }
}

function getValueDisplay(keyConfig) {
    if (!keyConfig) return '未配置';
    // 尝试匹配按键类型
    const typeInfo = KEY_CONFIG_SCHEMA.keyTypes[keyConfig.type];
    if (!typeInfo)
        return `type:${keyConfig.type}\n${keyConfig.value !== undefined ? `value:${keyConfig.value}` : ''}`;

    // 有匹配的按键类型，获取类型名
    let displayText = typeInfo.name;
    // 如果类型为带值类型，则应该显示值名
    if (typeInfo.hasValue) {
        // 如果传入配置中没有值，说明有错误
        if (keyConfig.value === undefined) {
            displayText = `${typeInfo.name}:valueError`;
        }
        // 传入配置中确定有值 
        else {
            // 尝试获取值名的集合
            let keyNames = KEY_CONFIG_SCHEMA.valueLabels[typeInfo.valueType];
            // 如果不存在值名集合，使用类型名:值作为显示文本，理论上不该存在这种情况 
            if (!keyNames) {
                displayText = `${typeInfo.name}:${keyConfig.value}`;
            }
            // 如果存在值名集合
            else {
                let keyName = keyNames[keyConfig.value];
                displayText = keyName ? keyName : `${typeInfo.name}:${keyConfig.value}`;
            }
        }
    } else {
        // 如果这个按键类型不需要值，不需要进一步处理
    }

    return displayText;
}

function closeConfigPanel(e) {
    if (activeConfigPanel) {
        // 检查是否点击在面板外部
        if (e && e.target.closest('.key-config-panel')) {
            return;
        }

        if (activeKeyElement) {
            activeKeyElement.classList.remove('active');
            activeKeyElement.removeChild(activeConfigPanel);
        }

        activeKeyElement = null;
        activeConfigPanel = null;
    }
}

// 工具函数

function findKeyConfig(configs, keyId, layer = null, ioType = null) {
    for (const config of configs) {
        if (layer !== null && config.layer !== layer) continue;
        if (ioType !== null && config.ioType !== ioType) continue;
        const keyConfig = config.keysArr.find(k => k.id === keyId);
        if (keyConfig) return keyConfig;
    }
    return null;
}

function getApiUrl(endpoint) {
    return `//${currentHost}${endpoint}`;
}

async function fetchWithTimeout(url, options = {}) {
    const { timeout = REQUEST_TIMEOUT, ...fetchOptions } = options;
    // 这里获取的url会有问题，以后有别的需求再改
    const fullUrl = url.startsWith('http') ? url : getApiUrl(url);

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), timeout);

    try {
        const response = await fetch(url, {
            ...fetchOptions,
            signal: controller.signal
        });
        return response;
    } finally {
        clearTimeout(timeoutId);
    }
}

// 应用配置，配置保存
async function saveKbdConfig() {
    const changed = getChangedConfig();
    if (changed.keyConfigsArr.length === 0) {
        showMessage("没有需要保存的更改");
        return;
    }

    try {
        const response = await fetchWithTimeout(getApiUrl(API_KBD_SET_CONFIG), {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(changed)
        });

        if (!response.ok) throw new Error(await response.text());

        // 更新默认配置
        defaultKbdConfig = JSON.parse(JSON.stringify(currentKbdConfig));
        updateUIState();
        showMessage("配置保存成功");
        // 保存后刷新页面从键盘获取最新配置，可能会导致currentHost被重置，不需要可以注释掉
        location.reload();
    } catch (error) {
        showMessage("保存失败: " + error.message);
    }
}

function getChangedConfig() {
    const changed = { keyConfigsArr: [] };

    if (!currentKbdConfig.keyConfigsArr || !defaultKbdConfig.keyConfigsArr) return changed;

    // 对比每个层的配置
    for (let layer = 0; layer < 3; layer++) {
        const currentLayerConfigs = currentKbdConfig.keyConfigsArr.filter(c => c.layer === layer);
        const defaultLayerConfigs = defaultKbdConfig.keyConfigsArr.filter(c => c.layer === layer);

        currentLayerConfigs.forEach(currentConfig => {
            const defaultConfig = defaultLayerConfigs.find(
                c => c.ioType === currentConfig.ioType
            );

            if (!defaultConfig) {
                // 新增的配置
                changed.keyConfigsArr.push(currentConfig);
                return;
            }

            // 找出有变化的按键
            const changedKeys = currentConfig.keysArr.filter(currentKey => {
                const defaultKey = defaultConfig.keysArr.find(k => k.id === currentKey.id);
                return !defaultKey || !deepEqual(currentKey, defaultKey);
            });

            if (changedKeys.length > 0) {
                changed.keyConfigsArr.push({
                    ioType: currentConfig.ioType,
                    layer: currentConfig.layer,
                    keysArr: changedKeys
                });
            }
        });
    }

    return changed;
}

function deepEqual(a, b) {
    return JSON.stringify(a) === JSON.stringify(b);
}

function resetKbdConfig() {
    if (confirm("确定要恢复默认配置吗？所有更改将丢失。")) {
        currentKbdConfig = JSON.parse(JSON.stringify(defaultKbdConfig));
        renderKeyboard();
        updateUIState();
    }
}

async function fetchLatestConfig() {
    if (getChangedConfig().keyConfigsArr.length > 0) {
        if (!confirm('当前有未保存的修改，获取最新配置将丢失这些更改。是否继续？')) {
            return;
        }
    }

    try {
        await loadKbdConfig();
        showMessage("配置已更新");
    } catch (error) {
        showMessage("获取配置失败: " + error.message);
    }
}

function updateUIState() {
    const hasChanges = getChangedConfig().keyConfigsArr.length > 0;
    saveBtn.disabled = !hasChanges;
    resetBtn.disabled = !hasChanges;
}

function showMessage(message) {
    alert(message);
}

function showError(message) {
    configContainer.innerHTML = `
        <div class="error">
            <p>${message}</p>
            <button onclick="loadKbdConfig()">重试</button>
        </div>
    `;
}