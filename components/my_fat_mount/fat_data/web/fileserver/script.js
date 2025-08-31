const MAX_FILE_SIZE = (0x350000);
const MAX_FILE_SIZE_STR = "3.4MB";
const hrefExtensionsArr = ["txt", "png", "ico", "md", "html", "htm", "js", "css", "json", "xml", "md5", "sha1", "sha256", "sha512", "pdf", "doc", "docx", "ppt", "c", "h", "py", "cpp", "hpp", "java", "php"];
const skipDirNameArr = [".", "..", "System Volume Information"];
// todo 添加特殊字符检测
window.addEventListener('hashchange', () => {
    let currentPath = window.location.hash;
    if (!currentPath.startsWith("#/") ||
        !currentPath.endsWith("/")) {
        window.location.hash = "#/";
    } else {
        loadFileList(window.location.hash.substring(1));
    }
});

// 页面加载完成后执行
document.addEventListener('DOMContentLoaded', () => {
    if (window.location.pathname.startsWith("/api/fileget")) {
        const params = new URLSearchParams(window.location.search);
        if (params.has("path")) {
            const filePath = params.get("path");
            setTimeout(() => {
                alert(`You are accessing path ${window.location.pathname}${window.location.search}${window.location.hash} with api:fileget, well auto redirect to ${filePath}.`);
            }, 0);
            window.location.href = filePath;
            return;
        }
        else {
            alert("Current url is startwith /api/fileget, and can't get path to redirect, may be occur undefined behavior.");
        }
    }
    let currentPath = window.location.hash;
    if (!currentPath.startsWith("#/") ||
        !currentPath.endsWith("/")) {
        window.location.hash = "#/";
    } else {
        loadFileList(window.location.hash.substring(1));
    }
});

// 设置文件路径，当选择要上传文件后会被触发
function setpath() {
    const uploadFileElement = document.getElementById("newfile");
    if (uploadFileElement.files.length === 0) {
        document.getElementById("filepath").value = "";
        return;
    } else {
        const uploadFileName = document.getElementById("newfile").files[0].name;
        const currentPath = window.location.hash.substring(1);
        document.getElementById("filepath").value = currentPath + uploadFileName;
    }
}

// 上传文件
function upload() {
    let filePath = document.getElementById("filepath").value;
    if (!filePath.startsWith('/')) {
        filePath = window.location.hash.substring(1) + filePath;
    }
    const uploadURI = "/api/fileupload?path=" + filePath;
    const fileInput = document.getElementById("newfile").files;

    if (fileInput.length === 0) {
        alert("No file selected!");
    } else if (filePath.length === 0) {
        alert("File path on server is not set!");
    } else if (filePath.indexOf(' ') >= 0) {
        alert("File path on server cannot have spaces!");
    } else if (filePath.endsWith('/')) {
        alert("can't upload a directory!");
    } else if (fileInput[0].size > MAX_FILE_SIZE) {
        alert("File size must be less than " + MAX_FILE_SIZE_STR + "!");
    } else {
        const result = confirm(`Upload file <${fileInput[0].name}> to: ${filePath} ?`);
        if (result === false) {
            return;
        }
        document.getElementById("newfile").disabled = true;
        document.getElementById("filepath").disabled = true;
        let uploadButton = document.getElementById("upload");
        uploadButton.innerHTML = "Uploading...";
        uploadButton.disabled = true;

        const file = fileInput[0];
        const xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = () => {
            if (xhttp.readyState === 4) {
                let fileElement = document.getElementById("newfile");
                let uploadButton = document.getElementById("upload");
                let pathElement = document.getElementById("filepath");
                pathElement.disabled = false;
                uploadButton.disabled = false;
                fileElement.disabled = false;
                uploadButton.innerHTML = "Upload";
                fileElement.value = "";
                pathElement.value = "";
                if (xhttp.status === 200) {
                    setTimeout(() => {
                        alert(`Upload file ${filePath} success!\n`);
                    }, 0);
                    loadFileList(window.location.hash.substring(1));
                } else {
                    alert(xhttp.status + ` Upload file ${filePath} Error!\n` + xhttp.responseText);
                }
            }
        };
        xhttp.open("POST", uploadURI, true);
        xhttp.send(file);
    }
}

// 加载文件列表，只接受文件夹作为路径
function loadFileList(path) {
    if (path.startsWith("#/")) {
        path = path.substring(1);
    }
    if (!path.startsWith('/')) {
        alert("path must be absolute path. (start with /)");
        return;
    }
    if (!path.endsWith('/')) {
        alert("path must be directory. (end with /)");
        return;
    }
    fetch(`/api/filelist?path=${path}`)
        .then(response => response.json())
        .then(data => {
            let pathLinkRow = document.getElementById("pathLinkRow");
            let dirPath = data.dirPath;
            dirPath = dirPath.slice(0, dirPath.lastIndexOf('/') + 1);
            dirPath = dirPath[0] === '/' ? dirPath : '/' + dirPath;
            let pathArray = dirPath.split('/');
            if (pathArray.length > 1) {
                pathLinkRow.innerText = '';
                pathLinkRow.innerHTML = '';
                let first_item = pathArray.shift();
                let pathLink = `/`;
                const firstLink = document.createElement('a');
                firstLink.href = window.location.pathname + "#" + pathLink;
                firstLink.textContent = `root:`;
                pathLinkRow.appendChild(firstLink);
                pathLinkRow.innerHTML += '/';

                pathArray.forEach(item => {
                    if (item.trim().length > 0) {
                        pathLink = pathLink + item + '/';
                        const fileLink = document.createElement('a');
                        fileLink.href = window.location.pathname + "#" + pathLink;
                        fileLink.textContent = item;
                        pathLinkRow.appendChild(fileLink);
                        pathLinkRow.innerHTML += '/';
                    }
                });
            } else {
                pathLinkRow.innerText = 'dir path format error';
            }
            const fileList = document.getElementById("fileList");
            fileList.innerHTML = '';
            data.fileList.forEach(item => {
                if (skipDirNameArr.includes(item.name)) return;
                const row = document.createElement("tr");

                // 文件名
                let targetPath = `${dirPath}${item.isDirectory ? item.name + '/' : item.name}`;
                const nameCell = document.createElement("td");
                const link = document.createElement("a");
                if (item.isDirectory) {
                    link.href = window.location.pathname + "#" + targetPath;
                } else { //如果是文件，使用/api/fileget?path=/path获取文件
                    link.href = `/api/fileget?path=${targetPath}`;
                    const ext = targetPath.split('.').pop().toLowerCase();
                    if (!hrefExtensionsArr.includes(ext)) {
                        link.onclick = () => {
                            if (!confirm(`可能为浏览器不支持展示的文件类型，如果不支持将触发下载文件，点击确定访问该文件`)) {
                                return false;
                            }
                            return true;
                        };
                    }
                }

                link.textContent = item.name + (item.isDirectory ? '/' : '');
                nameCell.appendChild(link);
                row.appendChild(nameCell);

                // 文件类型
                const typeCell = document.createElement("td");
                typeCell.textContent = item.isDirectory ? "Directory" : "File";
                row.appendChild(typeCell);

                // 文件大小
                const sizeCell = document.createElement("td");
                sizeCell.textContent = item.isDirectory ? '-' : item.size;
                row.appendChild(sizeCell);

                // 删除按钮
                const deleteCell = document.createElement("td");
                if (!item.isDirectory) {
                    const deleteButton = document.createElement("button");
                    deleteButton.textContent = "Delete";
                    deleteButton.onclick = () => deleteFile(targetPath);
                    deleteCell.appendChild(deleteButton);
                }
                row.appendChild(deleteCell);

                fileList.appendChild(row);
            });

        })
        .catch(error => {
            console.error("Error loading file list:", error);
            alert("Error loading file list");
            let rollbackPath = path.slice(0, path.lastIndexOf('/'));
            let last = rollbackPath.lastIndexOf('/');
            if (last <= 0) {
                rollbackPath = "/"
            } else {
                rollbackPath = rollbackPath.slice(0, last + 1);
            }
            window.location.hash = "#" + rollbackPath;
        });
}

// 删除文件
function deleteFile(path) {
    if (path.startsWith("#/")) {
        path = path.substring(1);
    }
    let confirmString;
    if (path === '/web/fileserver/index.html' ||
        path === '/web/fileserver/script.js' ||
        path === '/web/fileserver/style.css') {
        confirmString = `${path} is for file server. Confirm delete?`;

    } else {
        confirmString = `Delete ${path} ?`;
    }
    if (confirm(`${confirmString}`)) {
        fetch(`/api/filedelete?path=${path}`, { method: 'POST' })
            .then(response => {
                if (response.ok) {
                    // 重新加载文件列表
                    loadFileList(window.location.hash.substring(1));
                } else {
                    console.error("Failed to delete file");
                }
            })
            .catch(error => {
                console.error("Error deleting file:", error);
            });
    }
}