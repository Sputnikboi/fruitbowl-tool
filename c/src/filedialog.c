#include "filedialog.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef _WIN32

bool fb_pick_folder(char *out_path, int out_size) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "powershell -Command \"Add-Type -AssemblyName System.Windows.Forms; "
        "$f = New-Object System.Windows.Forms.FolderBrowserDialog; "
        "$f.Description = 'Select Resource Pack Folder'; "
        "if ($f.ShowDialog() -eq 'OK') { $f.SelectedPath }\"");
    FILE *p = _popen(cmd, "r");
    if (!p) return false;
    char buf[512] = {0};
    fgets(buf, sizeof(buf), p);
    _pclose(p);
    // Trim newline
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    if (len == 0) return false;
    strncpy(out_path, buf, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
}

bool fb_pick_bbmodel(char *out_path, int out_size) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "powershell -Command \"Add-Type -AssemblyName System.Windows.Forms; "
        "$f = New-Object System.Windows.Forms.OpenFileDialog; "
        "$f.Filter = 'Blockbench Models (*.bbmodel)|*.bbmodel|All Files (*.*)|*.*'; "
        "$f.Title = 'Select BBModel File'; "
        "if ($f.ShowDialog() -eq 'OK') { $f.FileName }\"");
    FILE *p = _popen(cmd, "r");
    if (!p) return false;
    char buf[512] = {0};
    fgets(buf, sizeof(buf), p);
    _pclose(p);
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    if (len == 0) return false;
    strncpy(out_path, buf, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
}

#else // Linux

bool fb_pick_folder(char *out_path, int out_size) {
    FILE *p = popen("zenity --file-selection --directory --title='Select Resource Pack Folder' 2>/dev/null", "r");
    if (!p) return false;
    char buf[512] = {0};
    fgets(buf, sizeof(buf), p);
    int status = pclose(p);
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    if (status != 0 || len == 0) return false;
    strncpy(out_path, buf, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
}

bool fb_pick_bbmodel(char *out_path, int out_size) {
    FILE *p = popen("zenity --file-selection --title='Select BBModel File' "
                    "--file-filter='Blockbench Models | *.bbmodel' "
                    "--file-filter='All Files | *' 2>/dev/null", "r");
    if (!p) return false;
    char buf[512] = {0};
    fgets(buf, sizeof(buf), p);
    int status = pclose(p);
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    if (status != 0 || len == 0) return false;
    strncpy(out_path, buf, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
}

#endif
