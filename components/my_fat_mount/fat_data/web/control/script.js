// 配置常量
const API_DEV = '/api/dev';
const API_FILELIST = '/api/filelist?path=';
const CHECK_INTERVAL = 180000; // 3分钟
const REQUEST_TIMEOUT = 10000; // 10秒

// 忽略的目录/文件名
const IGNORED_NAMES = [".", "..", "System Volume Information"];

// 友好名称映射
const NAME_MAPPING = {
    "/web/index.html": "用户默认页面",
    "/web/control/index.html": "导航页面（可能为当前页）"
};

// 默认导航链接
const DEFAULT_LINKS = [
    { path: "/web/fileserver/index.html", name: "文件服务器" },
    { path: "/web/control/dev.html", name: "设备设置" },
    { path: "/web/control/kbd.html", name: "键盘按键设置" }
];

// DOM元素
const statusIndicator = document.getElementById('status-indicator');
const statusText = document.getElementById('status-text');
const checkStatusBtn = document.getElementById('check-status-btn');
const defaultNav = document.getElementById('default-nav');
const dynamicNav = document.getElementById('dynamic-nav');
const scanPagesBtn = document.getElementById('scan-pages-btn');
const scanError = document.getElementById('scan-error');

// 状态变量
let isOnline = false;
let checkIntervalId = null;
let pendingRequest = null;
let isFirstLoad = true;

// 初始化页面
function init() {
    renderDefaultLinks();
    checkServerStatus(false);
    startAutoCheck();

    checkStatusBtn.addEventListener('click', () => checkServerStatus(true));
    scanPagesBtn.addEventListener('click', scanAdditionalPages);
}

// 添加默认导航页面
function renderDefaultLinks() {
    defaultNav.innerHTML = DEFAULT_LINKS.map(link =>
        `<li><a href="${link.path}">${link.name}</a></li>`
    ).join('');
}

// 检查服务器状态
async function checkServerStatus(needSetButton) {
    if (isFirstLoad) {
        setServerOnline(true);
        isFirstLoad = false;
        return;
    }
    if (pendingRequest) {
        if (needSetButton && checkStatusBtn.disabled !== true) {
            setButtonLoading(checkStatusBtn, true);
        }
        return;
    }
    try {
        if (needSetButton)
            setButtonLoading(checkStatusBtn, true);
        pendingRequest = fetchWithTimeout(API_DEV, { timeout: REQUEST_TIMEOUT });
        await pendingRequest;
        if (!isOnline)
            setServerOnline(true);
        if (!checkIntervalId) startAutoCheck();
    } catch (error) {
        if (isOnline)
            setServerOnline(false);
        console.error("服务器状态检查失败:", error);
    } finally {
        pendingRequest = null;
        setButtonLoading(checkStatusBtn, false);
    }
}

// 扫描其他页面
// 按理说这个操作也要
async function scanAdditionalPages() {
    if (pendingRequest) return;

    try {
        setButtonLoading(scanPagesBtn, true);
        scanError.textContent = '';

        // 扫描/web/目录
        const rootFiles = await fetchDirectory('/web/');
        let foundLinks = [];

        // 处理根目录下的index.html
        processFileList(rootFiles, '/web/', foundLinks);

        // 处理子目录
        for (const item of rootFiles.fileList) {
            if (item.isDirectory && !IGNORED_NAMES.includes(item.name)) {
                const subDirPath = `/web/${item.name}/`;
                try {
                    const subDirFiles = await fetchDirectory(subDirPath);
                    processFileList(subDirFiles, subDirPath, foundLinks);
                } catch (error) {
                    console.error(`扫描目录失败: ${subDirPath}`, error);
                    scanError.textContent += `无法扫描目录: ${subDirPath}\n`;
                }
            }
        }
        if (foundLinks.length === 0) {
            scanError.textContent = "没有找到其它页面";
        } else {
            renderDynamicLinks(foundLinks);
        }
        if (!isOnline) {
            setServerOnline(true);
        }
    } catch (error) {
        console.error("扫描页面失败:", error);
        if (error.name === 'AbortError') {
            scanError.textContent = "扫描失败: " + "超时";
            if (isOnline)
                setServerOnline(false);
        } else {
            scanError.textContent = "扫描失败: " + error.message;
            if (!isOnline) {
                setServerOnline(true);
            }
        }
    } finally {
        setButtonLoading(scanPagesBtn, false);
    }
}

// 处理文件列表
function processFileList(fileList, basePath, foundLinks) {
    for (const item of fileList.fileList) {
        if (!item.isDirectory && item.name === 'index.html') {
            const fullPath = basePath + item.name;
            const displayPath = basePath;

            if (DEFAULT_LINKS.some(link => link.path === fullPath)) {
                continue;
            }

            foundLinks.push({
                path: fullPath,
                displayPath: displayPath,
                name: NAME_MAPPING[fullPath] || fullPath
            });
            console.log(foundLinks);
        }
    }
}

// 渲染动态发现的链接
function renderDynamicLinks(links) {
    dynamicNav.innerHTML = links.map(link =>
        `<li><a href="${link.path}">${link.name}</a></li>`
    ).join('');
}

// 获取目录内容
async function fetchDirectory(path) {
    if (!path.endsWith('/')) path += '/';
    if (!path.startsWith('/')) path = '/' + path;

    try {
        pendingRequest = fetchWithTimeout(API_FILELIST + path, {
            timeout: REQUEST_TIMEOUT
        });
        const response = await pendingRequest;

        if (!response.ok) {
            throw new Error(`HTTP错误: ${response.status}`);
        }

        return await response.json();
    } finally {
        pendingRequest = null;
    }
}

// 带超时的fetch
function fetchWithTimeout(url, options = {}) {
    const { timeout = 10000 } = options;

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), timeout);

    return fetch(url, {
        ...options,
        signal: controller.signal
    }).finally(() => clearTimeout(timeoutId));
}

// 设置服务器状态
function setServerOnline(online) {
    isOnline = online;
    statusIndicator.className = online ? 'online' : 'offline';
    statusText.textContent = online ? '在线' : '离线';

    if (!online) {
        stopAutoCheck();
    }
}

// 开始自动检查
function startAutoCheck() {
    stopAutoCheck();
    checkIntervalId = setInterval(() => checkServerStatus(false), CHECK_INTERVAL);
}

// 停止自动检查
function stopAutoCheck() {
    if (checkIntervalId) {
        clearInterval(checkIntervalId);
        checkIntervalId = null;
    }
}

// 设置按钮加载状态
function setButtonLoading(button, isLoading) {
    button.disabled = isLoading;
    if (isLoading) {
        button.innerHTML = `<span class="spinner"></span> ${button.textContent}`;
    } else {
        button.textContent = button === checkStatusBtn ? '检查服务器状态' : '扫描其他页面';
    }
}

// 启动应用
document.addEventListener('DOMContentLoaded', init);