/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * stageprofiplugin.cpp
 * The StageProfi plugin for lla
 * Copyright (C) 2006-2007 Simon Newton
 */

#include <stdlib.h>
#include <stdio.h>

#include <llad/pluginadaptor.h>
#include <llad/preferences.h>
#include <llad/logger.h>

#include "stageprofiplugin.h"
#include "stageprofidevice.h"

#define STAGEPROFI_DEVICE "/dev/ttyUSB0"

#include <vector>


/*
 * Entry point to this plugin
 */
extern "C" Plugin* create(const PluginAdaptor *pa) {
  return new StageProfiPlugin(pa, LLA_PLUGIN_STAGEPROFI);
}

/*
 * Called when the plugin is unloaded
 */
extern "C" void destroy(Plugin* plug) {
  delete plug;
}


/*
 * Start the plugin
 *
 * Multiple devices now supported
 */
int StageProfiPlugin::start() {
  int sd;
  vector<string> *dev_nm_v;
  vector<string>::iterator it;
  StageProfiDevice *dev;

  if (m_enabled)
    return -1;

  // setup prefs
  if (load_prefs() != 0)
    return -1;

  // fetch device listing
  dev_nm_v = m_prefs->get_multiple_val("device");

  for (it = dev_nm_v->begin(); it != dev_nm_v->end(); ++it) {
    dev = new StageProfiDevice(this, "Stage Profi Device", *it);

    if (dev == NULL)
      continue;

    if (dev->start()) {
      delete dev;
      continue;
    }

    // register our descriptors, with us as the manager
    // this should really be fatal
    if ((sd = dev->get_sd()) >= 0)
      m_pa->register_fd( sd, PluginAdaptor::READ, dev, this);

    m_pa->register_device(dev);
    m_devices.insert(m_devices.end(), dev);
  }

  delete dev_nm_v;

  if (m_devices.size() > 0)
    m_enabled = true;
  else
    delete m_prefs;

  return 0;
}


/*
 * Stop the plugin
 *
 * @return 0 on sucess, -1 on failure
 */
int StageProfiPlugin::stop() {
  StageProfiDevice *dev;
  unsigned int i = 0;

  if (!m_enabled)
    return -1;

  for (i = 0; i < m_devices.size(); i++) {
    dev = m_devices[i];

    m_pa->unregister_fd( dev->get_sd(), PluginAdaptor::READ);

    if (dev->stop())
      continue;

    m_pa->unregister_device(dev);
    delete dev;
  }

  m_devices.clear();
  m_enabled = false;
  delete m_prefs;

  return 0;
}

/*
 * return the description for this plugin
 *
 */
string StageProfiPlugin::get_desc() const {
    return
"Stage Profi Plugin\n"
"----------------------------\n"
"\n"
"This plugin creates devices with one output port.\n"
"\n"
"--- Config file : lla-stageprofi.conf ---\n"
"\n"
"device = /dev/ttyUSB0\n"
"The device to use. Multiple devices are supported\n";
}


/*
 * Called if fd_action returns an error for one of our devices
 *
 */
int StageProfiPlugin::fd_error(int error, FDListener *listener) {
  StageProfiDevice *dev  = dynamic_cast<StageProfiDevice *> (listener);
  vector<StageProfiDevice *>::iterator iter;

  if (!dev) {
    Logger::instance()->log(Logger::WARN, "fd_error : dynamic cast failed");
    return 0;
  }

  // stop this device
  m_pa->unregister_fd(dev->get_sd(), PluginAdaptor::READ);

  // stop the device
  dev->stop();

  m_pa->unregister_device(dev);

  iter = find(m_devices.begin(), m_devices.end(), dev);
  if (*iter == dev)
    m_devices.erase(iter);

  delete dev;

  error = 0;
  return 0;
}


/*
 * load the plugin prefs and default to sensible values
 *
 */
int StageProfiPlugin::load_prefs() {
  if ( m_prefs != NULL)
    delete m_prefs;

  m_prefs = new Preferences("stageprofi");

  if (m_prefs == NULL)
    return -1;

  m_prefs->load();

  if ( m_prefs->get_val("device") == "") {
    m_prefs->set_val("device", STAGEPROFI_DEVICE);
    m_prefs->save();
  }

  // check if this saved correctly
  // we don't want to use it if null
  if (m_prefs->get_val("device") == "") {
    delete m_prefs;
    return -1;
  }

  return 0;
}
