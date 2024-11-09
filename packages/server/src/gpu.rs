use regex::Regex;
use std::fs;
use std::path::Path;
use std::process::Command;
use std::str;

#[derive(Debug, Eq, PartialEq)]
pub enum GPUVendor {
    UNKNOWN,
    INTEL,
    NVIDIA,
    AMD,
}

#[derive(Debug)]
pub struct GPUInfo {
    vendor: GPUVendor,
    card_path: String,
    render_path: String,
    device_name: String,
}

impl GPUInfo {
    pub fn vendor(&self) -> &GPUVendor {
        &self.vendor
    }

    pub fn vendor_string(&self) -> &str {
        match self.vendor {
            GPUVendor::INTEL => "Intel",
            GPUVendor::NVIDIA => "NVIDIA",
            GPUVendor::AMD => "AMD",
            GPUVendor::UNKNOWN => "Unknown",
        }
    }

    pub fn card_path(&self) -> &str {
        &self.card_path
    }

    pub fn render_path(&self) -> &str {
        &self.render_path
    }

    pub fn device_name(&self) -> &str {
        &self.device_name
    }
}

fn get_gpu_vendor(vendor_id: &str) -> GPUVendor {
    match vendor_id {
        "8086" => GPUVendor::INTEL,  // Intel
        "10de" => GPUVendor::NVIDIA, // NVIDIA
        "1002" => GPUVendor::AMD,    // AMD/ATI
        _ => GPUVendor::UNKNOWN,
    }
}

/// Retrieves a list of GPUs available on the system.
/// # Returns
/// * `Vec<GPUInfo>` - A vector containing information about each GPU.
pub fn get_gpus() -> Vec<GPUInfo> {
    let mut gpus = Vec::new();

    // Run lspci to get PCI devices related to GPUs
    let lspci_output = Command::new("lspci")
        .arg("-mmnn") // Get machine-readable output with IDs
        .output()
        .expect("Failed to execute lspci");

    let output = str::from_utf8(&lspci_output.stdout).unwrap();

    // Filter lines that mention VGA or 3D controller
    for line in output.lines() {
        if line.to_lowercase().contains("vga compatible controller")
            || line.to_lowercase().contains("3d controller")
            || line.to_lowercase().contains("display controller")
        {
            if let Some((pci_addr, vendor_id, device_name)) = parse_pci_device(line) {
                // Run udevadm to get the device path
                if let Some((card_path, render_path)) = get_dri_device_path(&pci_addr) {
                    let vendor = get_gpu_vendor(&vendor_id);
                    gpus.push(GPUInfo {
                        vendor,
                        card_path,
                        render_path,
                        device_name,
                    });
                }
            }
        }
    }

    gpus
}

/// Parses a line from the lspci output to extract PCI device information.
/// # Arguments
/// * `line` - A string slice that holds a line from the 'lspci -mmnn' output.
/// # Returns
/// * `Option<(String, String, String)>` - A tuple containing the PCI address, vendor ID, and device name if parsing is successful.
fn parse_pci_device(line: &str) -> Option<(String, String, String)> {
    // Define a regex pattern to match PCI device lines
    let re = Regex::new(r#"(?P<pci_addr>[0-9a-fA-F]{1,2}:[0-9a-fA-F]{2}\.[0-9]) "[^"]+" "(?P<vendor_name>[^"]+)" "(?P<device_name>[^"]+)"#).unwrap();

    // Collect all matched groups
    let parts: Vec<(String, String, String)> = re
        .captures_iter(line)
        .filter_map(|cap| {
            Some((
                cap["pci_addr"].to_string(),
                cap["vendor_name"].to_string(),
                cap["device_name"].to_string(),
            ))
        })
        .collect();

    if let Some((pci_addr, vendor_name, device_name)) = parts.first() {
        // Create a mutable copy of the device name to modify
        let mut device_name = device_name.clone();

        // If more than 1 square-bracketed item is found, remove last one, otherwise remove all
        if let Some(start) = device_name.rfind('[') {
            if let Some(_) = device_name.rfind(']') {
                device_name = device_name[..start].trim().to_string();
            }
        }

        // Extract vendor ID from vendor name (e.g., "Intel Corporation [8086]" or "Advanced Micro Devices, Inc. [AMD/ATI] [1002]")
        let vendor_id = vendor_name
            .split_whitespace()
            .last()
            .unwrap_or_default()
            .trim_matches(|c: char| !c.is_ascii_hexdigit())
            .to_string();

        return Some((pci_addr.clone(), vendor_id, device_name));
    }

    None
}

/// Retrieves the DRI device paths for a given PCI address.
/// Doubles as a way to verify the device is a GPU.
/// # Arguments
/// * `pci_addr` - A string slice that holds the PCI address.
/// # Returns
/// * `Option<(String, String)>` - A tuple containing the card path and render path if found.
fn get_dri_device_path(pci_addr: &str) -> Option<(String, String)> {
    // Construct the base directory in /sys/bus/pci/devices to start the search
    let base_dir = Path::new("/sys/bus/pci/devices");

    // Define the target PCI address with "0000:" prefix
    let target_addr = format!("0000:{}", pci_addr);

    // Search for a matching directory that contains the target PCI address
    for entry in fs::read_dir(base_dir).ok()?.flatten() {
        let path = entry.path();

        // Check if the path matches the target PCI address
        if path.to_string_lossy().contains(&target_addr) {
            // Look for any files under the 'drm' subdirectory, like 'card0' or 'renderD128'
            let drm_path = path.join("drm");
            if drm_path.exists() {
                let mut card_path = String::new();
                let mut render_path = String::new();
                for drm_entry in fs::read_dir(drm_path).ok()?.flatten() {
                    let file_name = drm_entry.file_name();
                    if let Some(name) = file_name.to_str() {
                        if name.starts_with("card") {
                            card_path = format!("/dev/dri/{}", name);
                        }
                        if name.starts_with("renderD") {
                            render_path = format!("/dev/dri/{}", name);
                        }
                        // If both paths are found, break the loop
                        if !card_path.is_empty() && !render_path.is_empty() {
                            break;
                        }
                    }
                }
                return Some((card_path, render_path));
            }
        }
    }

    None
}
