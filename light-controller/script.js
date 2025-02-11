document.addEventListener("DOMContentLoaded", function () {
    async function fetchStatus() {
        try {
            const response = await fetch("/status");
            if (!response.ok) throw new Error("Failed to fetch status");
            const data = await response.json();
            generateUI(data);
        } catch (error) {
            console.error("Error fetching status:", error);
        }
    }

    function generateUI(statusData) {
        document.body.innerHTML = ""; // Clear previous content
        document.title = statusData.name;

        const header = document.createElement("h1");
        header.textContent = statusData.name;
        header.style.textAlign = "center";
        document.body.appendChild(header);

        const infoContainer = document.createElement("div");
        infoContainer.style.border = "1px solid #ddd";
        infoContainer.style.borderRadius = "8px";
        infoContainer.style.padding = "16px";
        infoContainer.style.margin = "20px auto";
        infoContainer.style.maxWidth = "400px";
        infoContainer.style.boxShadow = "0 2px 4px rgba(0, 0, 0, 0.1)";
        infoContainer.innerHTML = `
      <p><strong>MAC:</strong> ${statusData.mac}</p>
      <p><strong>SSID:</strong> ${statusData.ssid}</p>
      <p><strong>SCRIPT:</strong> ${statusData.scriptURL}</p>
    `;
        document.body.appendChild(infoContainer);

        const deviceContainer = document.createElement("div");
        deviceContainer.id = "device-container";
        deviceContainer.style.display = "flex";
        deviceContainer.style.flexWrap = "wrap";
        deviceContainer.style.justifyContent = "center";
        document.body.appendChild(deviceContainer);

        statusData.devices.forEach((device) => {
            const deviceElement = document.createElement("div");
            deviceElement.classList.add("device");
            deviceElement.style.border = "1px solid #ddd";
            deviceElement.style.borderRadius = "8px";
            deviceElement.style.padding = "12px";
            deviceElement.style.margin = "8px";
            deviceElement.style.boxShadow = "0 2px 4px rgba(0, 0, 0, 0.1)";
            deviceElement.style.width = "200px";
            deviceElement.innerHTML = `
        <h3 style="margin: 8px 0;">${device.name}${device.alias ? ` (${device.alias})` : ""}</h3>
        <p style="margin: 4px 0;">PIN: ${device.pin}</p>
        <p style="margin: 4px 0;">STATUS: <strong>${device.status ? "ON" : "OFF"}</strong></p>
        <button class="toggle-btn" data-name="${device.name}" data-status="${device.status ? "off" : "on"}" style="padding: 8px 16px; background-color: ${device.status ? "#ff4d4d" : "#4caf50"}; color: white; border: none; border-radius: 4px; cursor: pointer;">
          TURN ${device.status ? "OFF" : "ON"}
        </button>
      `;
            deviceContainer.appendChild(deviceElement);
        });

        generateConfigForm(statusData);
    }

    function generateConfigForm(statusData) {
        const configForm = document.createElement("form");
        configForm.id = "config-form";
        configForm.style.margin = "20px auto";
        configForm.style.padding = "16px";
        configForm.style.border = "1px solid #ddd";
        configForm.style.borderRadius = "8px";
        configForm.style.maxWidth = "400px";
        configForm.style.boxShadow = "0 2px 4px rgba(0, 0, 0, 0.1)";
        configForm.innerHTML = `
      <h2 style="margin: 8px 0;">CONFIGURATION</h2>
      <label>
        mode:
        <select name="type" id="config-type" style="width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box;">
          <option value="wifi">wifi</option>
          <option value="name">name</option>
          <option value="auth">auth</option>
          <option value="alias">alias</option>
          <option value="script">script</option>
          <option value="ap-mode">ap-mode</option>
          <option value="reset">reset</option>
        </select>
      </label>
      <div id="config-fields"></div>
      <button type="submit" style="width: 100%; padding: 10px; background-color: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer;">APPLY</button>
    `;
        document.body.appendChild(configForm);

        const configType = document.getElementById("config-type");
        const configFields = document.getElementById("config-fields");

        configType.addEventListener("change", () => updateConfigFields(configType.value, statusData));
        updateConfigFields(configType.value, statusData);

        configForm.addEventListener("submit", async (event) => {
            event.preventDefault();
            const formData = new FormData(configForm);
            const params = {};
            formData.forEach((value, key) => {
                params[key] = value;
            });
            if (params.type === 'reset') {
                await resetConfig();
            } else {
                await setConfig(`/set/${params.type}?${new URLSearchParams(params).toString()}`);
            }
            fetchStatus();
        });
    }

    async function resetConfig() {
        try {
            const response = await fetch(`/reset`);
            if (response.ok) {
                alert('Configuration reset successfully! Device will restart.');
            } else {
                console.error('Failed to reset configuration');
            }
        } catch (error) {
            console.error('Error resetting configuration:', error);
        }
    }

    function updateConfigFields(type, statusData) {
        const configFields = document.getElementById("config-fields");
        configFields.innerHTML = "";  // Clear previous fields

        let fields = "";
        switch (type) {
            case "wifi":
                fields = `
          <label>ssid: <input type="text" name="ssid" required style="width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box;" /></label>
          <label>password: <input type="text" name="password" style="width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box;" /></label>
        `;
                break;
            case "auth":
                fields = `
          <label>username: <input type="text" name="username" style="width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box;" /></label>
          <label>password: <input type="text" name="password" style="width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box;" /></label>
        `;
                break;
            case "alias":
                fields = `
          <label>name: 
            <select name="name" style="width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box;">
              ${statusData.devices.map(device => `<option value="${device.name}">${device.name}</option>`).join('')}
            </select>
          </label>
          <label>value: <input type="text" name="value" style="width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box;" /></label>
        `;
                break;
            case "script":
                fields = `
          <label>value: <input type="text" name="value" required style="width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box;" /></label>
        `;
                break;
            case "name":
                fields = `
          <label>name: <input type="text" name="value" required style="width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box;" /></label>
        `;
                break;
            default:
                fields = "";
        }

        configFields.innerHTML = fields;
    }

    async function setDeviceStatus(name, status) {
        try {
            const response = await fetch(`/set/device?name=${name}&status=${status}`);
            if (response.ok) {
                fetchStatus();
            } else {
                console.error("Failed to set device status");
            }
        } catch (error) {
            console.error("Error setting device status:", error);
        }
    }

    async function setConfig(path) {
        try {
            const response = await fetch(path);
            if (response.ok) {
                console.log("Configuration set successfully");
            } else {
                console.error("Failed to set configuration");
            }
        } catch (error) {
            console.error("Error setting configuration:", error);
        }
    }

    document.body.addEventListener("click", (event) => {
        if (event.target.classList.contains("toggle-btn")) {
            const name = event.target.getAttribute("data-name");
            const status = event.target.getAttribute("data-status");
            setDeviceStatus(name, status);
        }
    });

    fetchStatus();
});