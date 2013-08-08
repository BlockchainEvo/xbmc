#pragma once

/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <utils/StrongPointer.h>
#include <MediaSource.h>

class DllLibStagefright;

class DllReadOptions
{
public:
  DllReadOptions();
  DllReadOptions(android::MediaSource::ReadOptions* options);
  ~DllReadOptions();

  void setSeekTo(int64_t time_us, android::MediaSource::ReadOptions::SeekMode mode = android::MediaSource::ReadOptions::SEEK_CLOSEST_SYNC);
  void clearSeekTo();
  bool getSeekTo(int64_t *time_us, android::MediaSource::ReadOptions::SeekMode *mode);
  void setLateBy(int64_t lateness_us);
  int64_t getLateBy();

  android::MediaSource::ReadOptions* get();
private:
  DllLibStagefright *m_dll;
  android::MediaSource::ReadOptions* m_options;
  bool m_inherited;
};
