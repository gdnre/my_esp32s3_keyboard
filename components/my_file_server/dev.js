// 配置元数据 - 可根据实际需求扩展
const CONFIG_SCHEMA = {
    "nBootMode": {
        displayName: "下次启动模式",
        type: "select",
        options: {
            0: "默认模式",
            1: "MSC模式"
        },
        displayGroup: "系统设置",
        displaySort: 1,
        visible: true
    },
    "bootMode": {
        displayName: "当前启动模式",
        type: "select",
        options: {
            0: "默认模式",
            1: "MSC模式"
        },
        displayGroup: "系统设置",
        displaySort: 2,
        visible: true,
        isConst: true,
        helpInfo: "仅查看当前启动模式用，不会影响下次启动模式，要修改下次启动模式，请修改<下次启动模式>选项（nBootMode）。"
    },
    "checkUpdate": {
        displayName: "检查更新",
        type: "select",
        options: {
            0: "不检查更新",
            1: "下次启动时尝试更新"
        },
        displayGroup: "系统设置",
        displaySort: 3,
        visible: true,
        helpInfo: "设置下次更新是否检查更新文件并尝试更新，必须存在update.bin文件且不存在UPDATE.OK文件时才会更新，每次检查后值都会被重置为0"
    },
    "outDef": {
        displayName: "输出默认配置",
        type: "boolean",
        displayGroup: "系统设置",
        displaySort: 4,
        visible: true,
        helpInfo: "是否在下次启动时，将默认配置写入到fat中，如果关闭，则永远不会尝试输出默认配置，如果开启但文件已存在，也不会输出，每次尝试输出后都会被重置为0"
    },
    "usb": {
        displayName: "usb键盘",
        type: "boolean",
        displayGroup: "键盘模式设置",
        displaySort: 4,
        visible: true,
    },
    "ble": {
        displayName: "ble键盘",
        type: "boolean",
        displayGroup: "键盘模式设置",
        displaySort: 4,
        visible: true,
    },
    "wifiMode": {
        displayName: "wifi模式",
        type: "select",
        options: {
            0: "关闭wifi",
            1: "仅STA模式 连接路由器",
            2: "仅AP模式 作为热点",
            3: "APSTA模式",
            8: "ESPNOW模式 类以键盘2.4g连接"
        },
        displayGroup: "键盘模式设置",
        displaySort: 3,
        visible: true,
        helpInfo: "注意关闭wifi后就无法进入该配置页面，ESPNOW模式不和其它wifi模式共存且需要另一个esp设备"
    },
    "display": {
        displayName: "屏幕开关",
        type: "boolean",
        displayGroup: "显示设置",
        displaySort: 1,
        visible: true
    },
    "brightness": {
        displayName: "屏幕亮度",
        type: "range",
        min: 0,
        max: 100,
        step: 1,
        unit: "%",
        displayGroup: "显示设置",
        displaySort: 2,
        visible: true
    },
    "lvScrIndex": {
        displayName: "默认屏幕序号",
        type: "select",
        options: {
            0: "屏幕0，有信息显示的屏幕",
            1: "屏幕1，仅显示可能有设置的图片"
        },
        displayGroup: "显示设置",
        displaySort: 3,
        visible: true
    },
    "bkImage0": {
        displayName: "背景图片0路径",
        type: "string",
        maxLength: 256,
        displayGroup: "显示设置",
        displaySort: 4,
        visible: false,
        helpInfo: "第一个屏幕，有信息显示的屏幕的背景图片路径，仅支持png图片"
    },
    "bkImage1": {
        displayName: "背景图片1路径",
        type: "string",
        maxLength: 256,
        displayGroup: "显示设置",
        displaySort: 5,
        visible: true,
        helpInfo: "第二个屏幕的背景图片路径，仅支持png图片"
    },
    "sleepEn": {
        displayName: "启用睡眠",
        type: "boolean",
        displayGroup: "电源管理",
        displaySort: 1,
        visible: true,
        helpInfo: "检测到usb已连接电源时不会睡眠。(设备会定期检测usb的vbus引脚电压，当电压高于一定值时不会触发睡眠，如果无操作超过一定时间，然后拔掉电源，且没有其它操作，会在下次检测电压时立刻睡眠)"

    },
    "sleepTime": {
        displayName: "无操作睡眠时间",
        type: "range",
        min: 0,
        max: 65535,
        step: 60,
        unit: "s",
        displayGroup: "电源管理",
        displaySort: 2,
        visible: true
    },
    "apSSID": {
        displayName: "AP热点名称",
        type: "string",
        maxLength: 32,
        displayGroup: "网络设置",
        displaySort: 1,
        visible: true,
        helpInfo: "如果要使用默认的设备名，请输入<NONE>"
    },
    "apPasswd": {
        displayName: "AP热点密码",
        type: "string",
        maxLength: 64,
        displayGroup: "网络设置",
        displaySort: 1,
        visible: true
    },
    "staSSID": {
        displayName: "wifi名称",
        type: "string",
        maxLength: 32,
        displayGroup: "网络设置",
        displaySort: 1,
        visible: true,
        helpInfo: "要连接的wifi名称"
    },
    "staPasswd": {
        displayName: "wifi密码",
        type: "string",
        maxLength: 64,
        displayGroup: "网络设置",
        displaySort: 1,
        visible: true,
        helpInfo: "要连接的wifi密码，如果要设置为空，请至少输入一个空格，或者通过文件服务器手动修改配置，不保证服务器能支持空格作为密码，也不建议连接无密码路由器，只对WPA2类型加密进行过可用性验证"
    },
    "logLevel": {
        displayName: "打印日志级别",
        type: "select",
        options: {
            0: "无日志 none",
            1: "错误 error",
            2: "警告 warning",
            3: "信息 info",
            4: "调试 debug"
        },
        displayGroup: "系统设置",
        displaySort: 2,
        visible: true,
        helpInfo: "如果在ESPNOW模式下，希望从深睡唤醒时会响应第一次按键，请设置成warning级别以下，否则可能启动太慢而无法响应第一个按键"
    },
    "ledMode": {
        displayName: "led灯模式",
        type: "select",
        options: {
            8: "关闭灯",
            0: "闪烁2下红色后熄灭",
            1: "闪烁3下绿色后熄灭",
            2: "白色循环呼吸，慢速",
            3: "白色循环呼吸，快速",
            4: "蓝色循环呼吸",
            5: "HSV颜色循环",
            6: "RGB颜色循环",
            7: "全部灯颜色循环（多个灯时）",
            9: "单色模式",
        },
        displayGroup: "LED灯设置",
        displaySort: 1,
        visible: true
    },
    "ledBri": {
        displayName: "led灯亮度百分比",
        type: "range",
        min: 0,
        max: 100,
        step: 1,
        unit: "%",
        displayGroup: "LED灯设置",
        displaySort: 2,
        visible: true,
        helpInfo: "仅在单色模式下有效，下面的颜色、校准值和色温同样，当设置为10%以下时，会自动限制到10%，如果要关闭led灯，请设置为关闭模式而非将亮度设置为0"
    },
    "ledColor": {
        displayName: "led灯颜色",
        type: "number",
        min: 0,
        max: 0xffffffff,
        displayGroup: "LED灯设置",
        displaySort: 3,
        visible: true,
        helpInfo: "RGB颜色，格式为0xIIRRGGBB，请自行转化为10进制，0xII指示led序号，序号格式为0bIIIIIIIx，第1位无效，后7位（即0b0IIIIIII）为127时表示所有led灯"
    },
    "ledCali": {
        displayName: "led灯颜色校准值",
        type: "number",
        min: 0,
        max: 0xffffff,
        displayGroup: "LED灯设置",
        displaySort: 4,
        visible: true,
        helpInfo: "当值大于0时生效，RGB格式的颜色校准值（0xRRGGBB），应用颜色时，会将各自通道的值*校准值再/255"
    },
    "ledTemp": {
        displayName: "led灯色温",
        type: "number",
        min: 0,
        max: 0xffffffff,
        displayGroup: "LED灯设置",
        displaySort: 5,
        visible: false,
        helpInfo: "设置色温，值范围0-1000000，设置为大于等于16777216时，则不会应用色温，尽量不要使用"
    }
};

// API路径
const API_GET_NVS_CONFIG = '/api/dev/nvsconfig'; // 获取配置的api
const API_SET_CONFIG = '/api/dev/setconfig';// 设置和上传配置的api
const API_DEV_STATUS = '/api/dev';// 查看设置状态的api
const REQUEST_TIMEOUT = 20000;

// 全局变量
let defaultConfig = {};
let currentConfig = {};
let configElements = {};
let isOnline = false;
let pendingRequest = null;
let currentHost = window.location.hostname;

// DOM元素
const statusIndicator = document.getElementById('status-indicator');
const statusText = document.getElementById('status-text');
const checkStatusBtn = document.getElementById('check-status-btn');
const configContainer = document.getElementById('config-container');
const setBtn = document.getElementById('save-btn');
const resetBtn = document.getElementById('reset-btn');
const uploadBtn = document.getElementById('upload-btn');
const configPathInput = document.getElementById('config-path');
const resultModal = document.getElementById('result-modal');
const resultContent = document.getElementById('result-content');
const closeModalBtn = document.getElementById('close-modal');
const fetchConfigBtn = document.getElementById('fetch-config-btn');
const hostInput = document.getElementById('host-input');

// 初始化
document.addEventListener('DOMContentLoaded', init);

function init() {
    hostInput.value = currentHost;
    setupEventListeners();
    checkServerStatus();
    loadConfig();
}

function setupEventListeners() {
    checkStatusBtn.addEventListener('click', checkServerStatus);
    setBtn.addEventListener('click', setConfig);
    resetBtn.addEventListener('click', resetConfig);
    uploadBtn.addEventListener('click', uploadConfig);
    closeModalBtn.addEventListener('click', () => {
        resultModal.classList.add('hidden');
    });
    fetchConfigBtn.addEventListener('click', fetchLatestConfig);
    hostInput.addEventListener('change', (e) => {
        currentHost = e.target.value.trim() || window.location.hostname;
        if (hostInput.value !== currentHost) { hostInput.value = currentHost; }
    });
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
        showResult(`服务器连接失败: ${error.message}`);
    } finally {
        pendingRequest = null;
        setButtonLoading(checkStatusBtn, false);
    }
}

function setServerOnline(online) {
    isOnline = online;
    statusIndicator.className = online ? 'online' : 'offline';
    statusText.textContent = online ? '在线' : '离线';
    updateUIState();
}

// 加载配置
async function loadConfig() {
    try {
        const response = await fetchWithTimeout(getApiUrl(API_GET_NVS_CONFIG), { timeout: REQUEST_TIMEOUT });
        if (!response.ok) throw new Error(`HTTP错误: ${response.status}`);

        const config = await response.json();
        if (!config.devConfigsObj) throw new Error("无效的配置格式");

        defaultConfig = JSON.parse(JSON.stringify(config.devConfigsObj));
        currentConfig = JSON.parse(JSON.stringify(config.devConfigsObj));

        renderConfigForm();
        updateUIState();
    } catch (error) {
        console.error("加载配置失败:", error);
        configContainer.innerHTML = `
            <div class="error">
                <p>加载配置失败: ${error.message}</p>
                <button id="retry-btn">重试</button>
            </div>
        `;
        document.getElementById('retry-btn').addEventListener('click', loadConfig);
    }
}

// 渲染配置表单
function renderConfigForm() {
    // 按displayGroup分组
    const groupedConfigs = {};

    // 处理已定义的配置项
    for (const [key, schema] of Object.entries(CONFIG_SCHEMA)) {
        if (!schema.visible) continue;

        const group = schema.displayGroup || "其他设置";
        if (!groupedConfigs[group]) {
            groupedConfigs[group] = [];
        }

        groupedConfigs[group].push({
            key,
            schema,
            sort: schema.displaySort || 999
        });
    }

    // 处理未定义的配置项
    const undefinedGroup = "未分类配置";
    for (const key in currentConfig) {
        if (!CONFIG_SCHEMA[key] && currentConfig[key] !== undefined) {
            if (!groupedConfigs[undefinedGroup]) {
                groupedConfigs[undefinedGroup] = [];
            }

            groupedConfigs[undefinedGroup].push({
                key,
                schema: null,
                sort: 999
            });
        }
    }

    // 生成HTML
    let html = '';
    for (const [groupName, items] of Object.entries(groupedConfigs)) {
        // 按displaySort排序
        items.sort((a, b) => a.sort - b.sort);

        html += `
            <div class="config-group">
                <div class="config-group-header">
                    ${groupName}
                </div>
                <div class="config-group-content">
                    ${items.map(item => renderConfigItem(item.key, item.schema)).join('')}
                </div>
            </div>
        `;
    }

    configContainer.innerHTML = html;

    // 添加折叠事件
    document.querySelectorAll('.config-group-header').forEach(header => {
        header.addEventListener('click', () => {
            header.parentElement.classList.toggle('collapsed');
        });
    });

    // 初始化控件事件监听
    for (const key in currentConfig) {
        const element = document.querySelector(`[data-config-key="${key}"]`);
        if (!element) continue;

        configElements[key] = {
            element,
            container: element.closest('.config-item'),
            hintElement: element.parentNode.querySelector('.range-hint')
        };

        element.addEventListener('input', handleConfigChange);
        element.addEventListener('change', handleConfigChange);
    }
}

// 渲染单个配置项
function renderConfigItem(key, schema) {
    const value = currentConfig[key];
    const effectiveType = schema ? schema.type : inferType(value);

    // 常量属性
    const isConst = schema?.isConst || false;
    // 帮助信息
    const helpInfo = schema?.helpInfo || '';
    let controlHtml = '';
    let hintHtml = '';
    const rangeCheck = schema ? checkRange(key, value, schema) : { isValid: true };


    switch (effectiveType) {
        case 'boolean':
            controlHtml = `
                <label class="switch">
                    <input type="checkbox" data-config-key="${key}" 
                           ${value ? 'checked' : ''}>
                    <span class="slider"></span>
                </label>
            `;
            break;

        case 'range':
            const { min = 0, max = 100, step = 1, unit = '' } = schema || {};
            controlHtml = `
                <div class="control-row">
                    <input type="range" data-config-key="${key}" 
                           min="${min}" max="${max}" step="${step}" value="${value}">
                    <input type="number" data-config-key="${key}" 
                           min="${min}" max="${max}" step="${step}" value="${value}">
                    ${unit ? `<span class="unit-label">${unit}</span>` : ''}
                </div>
            `;
            if (!rangeCheck.isValid) {
                hintHtml = `<div class="range-hint">${rangeCheck.message}</div>`;
            }
            break;

        case 'select':
            const options = schema?.options || {};
            controlHtml = `
                <select data-config-key="${key}">
                    ${Object.entries(options).map(([optValue, optText]) => `
                        <option value="${optValue}" ${value == optValue ? 'selected' : ''}>
                            ${optText}
                        </option>
                    `).join('')}
                </select>
            `;
            break;

        case 'number':
            controlHtml = `
                <input type="number" data-config-key="${key}" value="${value}">
            `;
            break;

        default: // string
            const maxLength = schema?.maxLength || 255;
            controlHtml = `
                <input type="text" data-config-key="${key}" 
                       value="${value}" maxlength="${maxLength}">
            `;
    }

    const displayName = schema?.displayName || key;
    const isOutOfRange = !rangeCheck.isValid;
    // 添加帮助图标
    let helpIcon = '';
    if (helpInfo) {
        helpIcon = `
            <span class="help-icon" title="${helpInfo}">?</span>
        `;
    }
    // return `
    //     <div class="config-item ${isOutOfRange ? 'out-of-range' : ''}" data-config-key="${key}">
    //         <label class="config-label">${displayName}</label>
    //         ${controlHtml}
    //         ${hintHtml}
    //     </div>
    // `;
    return `
        <div class="config-item ${isOutOfRange ? 'out-of-range' : ''}" data-config-key="${key}">
            <label class="config-label" 
                   data-config-key="${key}"
                   ${!schema?.helpInfo ? `title="配置键: ${key}"` : ''}>
                ${displayName}
            </label>
            <div class="control-wrapper">
                ${controlHtml}
                ${helpIcon}
            </div>
            ${hintHtml}
        </div>
    `;
}

// 配置变更处理
function handleConfigChange(event) {
    const key = event.target.dataset.configKey;
    let newValue;

    if (event.target.type === 'checkbox') {
        newValue = event.target.checked ? 1 : 0;// 这里需要把bool值转为0或1，cjson处理时不会将bool识别为0或1
    } else if (event.target.type === 'number' || event.target.type === 'range' ||
        event.target.type === 'select' || event.target instanceof HTMLSelectElement) {
        newValue = parseFloat(event.target.value);
    } else {
        newValue = event.target.value;
    }

    // 更新关联控件（如range和number绑定）
    if (configElements[key]) {
        const linkedElements = document.querySelectorAll(`[data-config-key="${key}"]`);
        linkedElements.forEach(el => {
            if (el !== event.target) {
                if (el.type === 'checkbox') {
                    el.checked = newValue;
                } else {
                    el.value = newValue;
                }
            }
        });
    }

    currentConfig[key] = newValue;
    updateUIState();
}

// 应用配置
async function setConfig() {
    const changedConfig = getChangedConfig();
    if (Object.keys(changedConfig).length === 0) {
        showResult("没有需要保存的更改");
        return;
    }

    try {
        setButtonLoading(setBtn, true);
        let url = getApiUrl(API_SET_CONFIG) + "?type=nvs";//restart=n 添加前面的参数可以不重启设备，但部分配置需要重启才能应用
        const response = await fetchWithTimeout(url, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ devConfigsObj: changedConfig }),
            timeout: REQUEST_TIMEOUT
        });

        if (!response.ok) throw new Error(`HTTP错误: ${response.status}`);

        // 更新默认配置为当前值
        for (const key in changedConfig) {
            defaultConfig[key] = currentConfig[key];
        }
        // updateUIState();
        const result = await response.text();
        showResult(`配置应用成功:\n${result}`);
        fetchLatestConfig();
    } catch (error) {
        console.error("应用配置失败:", error);
        showResult(`应用配置失败: ${error.message}`);
    } finally {
        setButtonLoading(setBtn, false);
    }
}

// 恢复默认配置
function resetConfig() {
    if (confirm("确定要恢复默认配置吗？所有更改将丢失。")) {
        currentConfig = JSON.parse(JSON.stringify(defaultConfig));

        // 更新所有控件值
        for (const key in currentConfig) {
            if (configElements[key]) {
                const elements = document.querySelectorAll(`[data-config-key="${key}"]`);
                elements.forEach(el => {
                    if (el.type === 'checkbox') {
                        el.checked = currentConfig[key];
                    } else {
                        el.value = currentConfig[key];
                    }
                });

                // 更新范围验证状态
                const schema = CONFIG_SCHEMA[key];
                if (schema) {
                    const rangeCheck = checkRange(key, currentConfig[key], schema);
                    configElements[key].container.classList.toggle('out-of-range', !rangeCheck.isValid);
                    if (configElements[key].hintElement) {
                        configElements[key].hintElement.textContent =
                            rangeCheck.isValid ? '' : rangeCheck.message;
                    }
                }
            }
        }

        updateUIState();
    }
}

// 上传配置到指定路径并应用配置
async function uploadConfig() {
    const path = configPathInput.value.trim();

    if (!validateConfigPath(path)) {
        return;
    }

    const changedConfig = getChangedConfig();
    if (Object.keys(changedConfig).length === 0) {
        showResult("没有需要上传的更改");
        return;
    }

    try {
        setButtonLoading(uploadBtn, true);

        const url = getApiUrl(API_SET_CONFIG) + `?path=${encodeURIComponent(path)}&type=nvs`;//restart=n 添加前面的参数可以不重启设备，但部分配置需要重启才能应用
        const response = await fetchWithTimeout(url, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ devConfigsObj: changedConfig }),
            timeout: REQUEST_TIMEOUT
        });

        if (!response.ok) {
            const errorText = await response.text();
            throw new Error(`服务器返回错误: ${response.status} - ${errorText}`);
        }

        const result = await response.text();
        showResult(`配置上传成功:\n${result}`);
        fetchLatestConfig();
    } catch (error) {
        console.error("上传配置失败:", error);
        showResult(`上传配置失败: ${error.message}`);
    } finally {
        setButtonLoading(uploadBtn, false);
    }
}

// 辅助函数
function inferType(value) {
    if (typeof value === 'boolean') return 'boolean';
    if (typeof value === 'number') return 'number';
    return 'string';
}

function checkRange(key, value, schema) {
    if (schema.type !== 'range') return { isValid: true };

    const { min, max } = schema;
    if (typeof min !== 'undefined' && typeof max !== 'undefined') {
        const isValid = value >= min && value <= max;
        return {
            isValid,
            message: `应在 ${min}~${max}${schema.unit || ''} 之间`
        };
    }
    return { isValid: true };
}

function getChangedConfig() {
    const changed = {};
    for (const key in currentConfig) {
        if (!deepEqual(currentConfig[key], defaultConfig[key])) {
            changed[key] = currentConfig[key];

        }
    }
    return changed;
}

function deepEqual(a, b) {
    return JSON.stringify(a) === JSON.stringify(b);
}

function validateConfigPath(path) {
    if (!path.startsWith('/')) {
        alert("路径必须是绝对路径（以/开头）");
        return false;
    }
    if (path.endsWith('/')) {
        alert("路径不能是目录（不能以/结尾）");
        return false;
    }
    return true;
}

function updateUIState() {
    const hasChanges = Object.keys(getChangedConfig()).length > 0;
    setBtn.disabled = !hasChanges || !isOnline;
    resetBtn.disabled = !hasChanges;
    uploadBtn.disabled = !hasChanges || !isOnline;
}

function setButtonLoading(button, isLoading) {
    button.disabled = isLoading;
    if (isLoading) {
        button.innerHTML = `<span class="spinner"></span> ${button.textContent}`;
    } else {
        button.textContent = button.dataset.originalText || button.textContent;
    }
}

function showResult(message) {
    resultContent.textContent = message;
    resultModal.classList.remove('hidden');
}

// 带超时的fetch
function fetchWithTimeout(url, options = {}) {
    const fullUrl = url.startsWith('http') ? url : getApiUrl(url);
    const { timeout = 30000, ...fetchOptions } = options;

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), timeout);

    return fetch(url, {
        ...fetchOptions,
        signal: controller.signal
    }).finally(() => clearTimeout(timeoutId));
}


// 传入路径，根据当前主机名组合成url
function getApiUrl(endpoint) {
    return `//${currentHost}${endpoint}`;
}

// 更新当前的配置，从服务器请求最新配置并覆盖本地配置
async function fetchLatestConfig() {
    if (Object.keys(getChangedConfig()).length > 0) {
        if (!confirm('当前有未保存的修改，获取最新配置将丢失这些更改。是否继续？')) {
            return;
        }
    }

    try {
        setButtonLoading(document.getElementById('fetch-config-btn'), true);
        const response = await fetchWithTimeout(getApiUrl(API_GET_NVS_CONFIG), {
            timeout: REQUEST_TIMEOUT
        });

        if (!response.ok) throw new Error(`HTTP错误: ${response.status}`);

        const config = await response.json();
        defaultConfig = JSON.parse(JSON.stringify(config.devConfigsObj));
        currentConfig = JSON.parse(JSON.stringify(config.devConfigsObj));

        renderConfigForm();
        updateUIState();
        // showResult("配置更新成功");
    } catch (error) {
        console.error("获取配置失败:", error);
        showResult(`获取配置失败: ${error.message}， 如果在应用配置后出现，可能是因为设备重启后还未联网，可以手动刷新页面或点击<获取最新配置>按钮`);
    } finally {
        setButtonLoading(document.getElementById('fetch-config-btn'), false);
    }
}