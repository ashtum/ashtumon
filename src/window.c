#include "window.h"
#include "ashmon.h"
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static void window_update_opacity(struct window_display_struct *win_prop)
{
    unsigned int real_opacity = 0xFFFFFFFF;
    real_opacity *= (win_prop->opacity / 100.0);

    XChangeProperty(win_prop->display, win_prop->window, XInternAtom(win_prop->display, "_NET_WM_WINDOW_OPACITY", False),
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&real_opacity, 1L);
    XFlush(win_prop->display);
}

static void window_fadeout_fade_in_with_delay(struct window_display_struct *win_prop, int delay)
{
    int last_opacity = win_prop->opacity;
    while (win_prop->opacity > 0)
    {
        win_prop->opacity--;
        window_update_opacity(win_prop);
        usleep(3000);
    }
    XUnmapWindow(win_prop->display, win_prop->window);
    sleep(delay);
    win_prop->opacity = last_opacity;
    window_update_opacity(win_prop);
    XMapWindow(win_prop->display, win_prop->window);
}

static void long_rate_to_readable_str(char * prefix, char *p_buff, char per_second, double rate)
{
    int i = 0;
    char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    while (rate > 1000)
    {
        rate /= 1000;
        i++;
    }

    if (per_second)
        sprintf(p_buff, "%s%.*f %s/s", prefix, i, rate, units[i]);
    else
        sprintf(p_buff, "%s%.*f %s", prefix, i, rate, units[i]);
}

void window_init(struct window_display_struct *win_prop)
{
    XInitThreads();
    win_prop->display = XOpenDisplay(0);

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(win_prop->display, DefaultScreen(win_prop->display), 32, TrueColor, &vinfo))
        XMatchVisualInfo(win_prop->display, DefaultScreen(win_prop->display), 24, TrueColor, &vinfo);

    XSetWindowAttributes attr;
    attr.colormap = XCreateColormap(win_prop->display, DefaultRootWindow(win_prop->display), vinfo.visual, AllocNone);
    attr.border_pixel = 0;
    attr.background_pixel = 0;
    win_prop->window = XCreateWindow(win_prop->display, DefaultRootWindow(win_prop->display), 0, 0, 176, 87, 0,
                                     vinfo.depth, InputOutput, vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel, &attr);

    win_prop->gc = XCreateGC(win_prop->display, win_prop->window, 0, 0);

    XStoreName(win_prop->display, win_prop->window, (char *)"ashmon");
    XSelectInput(win_prop->display, win_prop->window, ExposureMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

    Atom property = XInternAtom(win_prop->display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(win_prop->display, win_prop->window, XInternAtom(win_prop->display, "_NET_WM_WINDOW_TYPE", False),
                    XA_ATOM, 32, PropModeReplace, (unsigned char *)&property, 1);

    XMoveWindow(win_prop->display, win_prop->window, win_prop->last_window_x, win_prop->last_window_y);
    XMapWindow(win_prop->display, win_prop->window);
    XSetLineAttributes(win_prop->display, win_prop->gc, 1, LineSolid, CapNotLast, JoinMiter);
    window_update_opacity(win_prop);
}

void window_redraw(struct window_display_struct *win_prop, struct nic_statistics *nic_s)
{
    int i;
    unsigned int line_h;
    char str_buffer[32];
    const int upload_color = 0xFFDC143C;
    const int download_color = 0xFF228B22;
    const int upload_download_color = 0xFFFFA500;
    const int background_color = 0xEEEEEEEE;
    const int primary_color = 0xFF1A90FF;

    XEvent exppp;
    memset(&exppp, 0, sizeof(exppp));
    exppp.type = Expose;
    exppp.xexpose.window = win_prop->window;
    XSendEvent(win_prop->display, win_prop->window, False, ExposureMask, &exppp);

    // remove shapes
    XSetForeground(win_prop->display, win_prop->gc, background_color);
    XFillRectangle(win_prop->display, win_prop->window, win_prop->gc, 0, 0, 176, 87);

    // primry shape draws
    XSetForeground(win_prop->display, win_prop->gc, primary_color);
    XDrawLine(win_prop->display, win_prop->window, win_prop->gc, 5, 55, 171, 55);
    XDrawRectangle(win_prop->display, win_prop->window, win_prop->gc, 0, 0, 175, 86);
    for (i = 0; i < 3; i++)
    {
        char str_buffer[32];
        long_rate_to_readable_str("", str_buffer, 1, (3.0 - i) * (nic_s->bigest_rate / 3.0));
        XDrawString(win_prop->display, win_prop->window, win_prop->gc, 5, 6 + i * 8 + (i+1) * 9, str_buffer, strlen(str_buffer));
    }

    long_rate_to_readable_str("TU ", str_buffer, 0, nic_s->total_tx_bytes);
    XDrawString(win_prop->display, win_prop->window, win_prop->gc, 88, 83, str_buffer, strlen(str_buffer));

    long_rate_to_readable_str("TD ", str_buffer, 0, nic_s->total_rx_bytes);
    XDrawString(win_prop->display, win_prop->window, win_prop->gc, 5, 83, str_buffer, strlen(str_buffer));

    // red shape draws
    XSetForeground(win_prop->display, win_prop->gc, upload_color);
    long_rate_to_readable_str("U  ", str_buffer, 1, nic_s->tx_rates[0]);
    XDrawString(win_prop->display, win_prop->window, win_prop->gc, 88, 69, str_buffer, strlen(str_buffer));
    for (i = 0; i < 100; i++)
    {
        if (nic_s->tx_rates[i] > 0)
            line_h = ((nic_s->tx_rates[i]) / (nic_s->bigest_rate * 1.0)) * 50.0;
        else
            line_h = 0;

        if (line_h > 0 && nic_s->tx_rates[i] >= nic_s->rx_rates[i])
            XDrawLine(win_prop->display, win_prop->window, win_prop->gc, 70 + (100 - i), 55, 70 + (100 - i), 5 + (50 - line_h));
    }
    
    // green shape draws
    XSetForeground(win_prop->display, win_prop->gc, download_color);
    long_rate_to_readable_str("D  ", str_buffer, 1, nic_s->rx_rates[0]);
    XDrawString(win_prop->display, win_prop->window, win_prop->gc, 5, 69, str_buffer, strlen(str_buffer));
    for (i = 0; i < 100; i++)
    {
        if (nic_s->rx_rates[i] > 0)
            line_h = ((nic_s->rx_rates[i]) / (nic_s->bigest_rate * 1.0)) * 50.0;
        else
            line_h = 0;

        if (line_h > 0 && nic_s->tx_rates[i] < nic_s->rx_rates[i])
            XDrawLine(win_prop->display, win_prop->window, win_prop->gc, 70 + (100 - i), 55, 70 + (100 - i), 5 + (50 - line_h));
    }

    // yelow shape draws
    XSetForeground(win_prop->display, win_prop->gc, upload_download_color);
    for (i = 0; i < 100; i++)
    {

        if (nic_s->tx_rates[i] > nic_s->rx_rates[i])
            line_h = (nic_s->rx_rates[i] / (nic_s->bigest_rate * 1.0)) * 50.0;
        else
            line_h = (nic_s->tx_rates[i] / (nic_s->bigest_rate * 1.0)) * 50.0;

        if (line_h > 0)
            XDrawLine(win_prop->display, win_prop->window, win_prop->gc, 70 + (100 - i), 55, 70 + (100 - i), 5 + (50 - line_h));
    }
    XFlush(win_prop->display);
}

void window_blocking_event_handler(struct window_display_struct *win_prop, struct nic_statistics *nic_s, char *cfg_path)
{
    char grabed = 0;
    char moved = 0;
    int last_mouse_x, last_mouse_y;
    XEvent ev;
    while (1)
    {
        XNextEvent(win_prop->display, &ev);

        if (ev.type == 4)
        {
            if (ev.xbutton.button == Button1)
            {
                Window window_returned;
                int tmp_x, tmp_y;
                unsigned int mask_return;
                grabed = 1;
                moved = 0;
                XQueryPointer(win_prop->display, DefaultRootWindow(win_prop->display),
                              &window_returned, &window_returned, &last_mouse_x,
                              &last_mouse_y, &tmp_x, &tmp_y, &mask_return);

                XTranslateCoordinates(win_prop->display, win_prop->window,
                                      DefaultRootWindow(win_prop->display), 0, 0,
                                      &win_prop->last_window_x, &win_prop->last_window_y,
                                      &window_returned);
            }
            else if (ev.xbutton.button == Button4)
            {
                if (win_prop->opacity <= 90)
                {
                    win_prop->opacity += 10;
                    window_update_opacity(win_prop);
                    save_configs_in_file(win_prop, nic_s, cfg_path);
                }
            }
            else if (ev.xbutton.button == Button5)
            {
                if (win_prop->opacity >= 20)
                {
                    win_prop->opacity -= 10;
                    window_update_opacity(win_prop);
                    save_configs_in_file(win_prop, nic_s, cfg_path);
                }
            }
            else if (ev.xbutton.button == Button3)
            {
                exit(0);
            }
        }
        if (ev.type == 5)
        {
            if (ev.xbutton.button == 1)
            {
                grabed = 0;
                if (!moved)
                {
                    window_fadeout_fade_in_with_delay(win_prop, 5);
                }
                else
                {
                    Window window_returned;
                    XTranslateCoordinates(win_prop->display, win_prop->window,
                                          DefaultRootWindow(win_prop->display), 0, 0,
                                          &win_prop->last_window_x, &win_prop->last_window_y,
                                          &window_returned);
                    save_configs_in_file(win_prop, nic_s, cfg_path);
                }
            }
        }
        if (ev.type == 6 && grabed == 1)
        {
            Window window_returned;
            int tmp_x, tmp_y, current_mouse_x, current_mouse_y;
            unsigned int mask_return;

            XQueryPointer(win_prop->display, DefaultRootWindow(win_prop->display),
                          &window_returned, &window_returned, &current_mouse_x,
                          &current_mouse_y, &tmp_x, &tmp_y, &mask_return);

            XMoveWindow(win_prop->display, win_prop->window,
                        win_prop->last_window_x + (current_mouse_x - last_mouse_x),
                        win_prop->last_window_y + (current_mouse_y - last_mouse_y));

            moved = 1;
        }
    }
}