/****************************************************************************
**
** Apple Push Notification Service daemon
**
** Copyright (C) 2014 DreamLogics <info@dreamlogics.com>
** Copyright (C) 2014 Stefan Ladage <sladage@gmail.com>
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published
** by the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/
#ifndef SHARED_H
#define SHARED_H

/*
 * 100 items max
 * 256 char json str max +1 for \0
 */

#define PAYLOAD_ARRAY_SIZE 100
#define PAYLOAD_JSONSTR_SIZE 257

struct PayloadData
{
    char device[64];
    char json[PAYLOAD_JSONSTR_SIZE];
};

struct SharedPayload
{
    PayloadData data[PAYLOAD_ARRAY_SIZE];
    int size;

    SharedPayload() : size(0) {}
};

#endif // SHARED_H
