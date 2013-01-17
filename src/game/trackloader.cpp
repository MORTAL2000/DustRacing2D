// This file is part of Dust Racing 2D.
// Copyright (C) 2011 Jussi Lind <jussi.lind@iki.fi>
//
// Dust Racing 2D is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// Dust Racing 2D is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Dust Racing 2D. If not, see <http://www.gnu.org/licenses/>.

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QDomDocument>
#include <QDomElement>

#include "layers.hpp"

#include "../common/targetnodebase.hpp"

#include "renderer.hpp"
#include "settings.hpp"
#include "track.hpp"
#include "trackdata.hpp"
#include "trackloader.hpp"
#include "trackobject.hpp"
#include "tracktile.hpp"

#include <MCAssetManager>
#include <MCLogger>
#include <MCObjectFactory>
#include <MCShapeView>

#include <algorithm>
#include <cassert>

TrackLoader * TrackLoader::m_instance = nullptr;

TrackLoader::TrackLoader(MCObjectFactory & objectFactory)
: m_trackObjectFactory(objectFactory)
, m_paths()
, m_tracks()
{
    assert(!TrackLoader::m_instance);
    TrackLoader::m_instance = this;
}

TrackLoader & TrackLoader::instance()
{
    assert(TrackLoader::m_instance);
    return *m_instance;
}

void TrackLoader::addTrackSearchPath(QString path)
{
    m_paths.push_back(path);
}

int TrackLoader::loadTracks()
{
    int numLoaded = 0;
    for (QString path : m_paths)
    {
        MCLogger().info() << "Loading race tracks from '" << path.toStdString() << "'..";
        QStringList trackPaths(QDir(path).entryList(QStringList("*.trk")));
        for (QString trackPath : trackPaths)
        {
            trackPath = path + QDir::separator() + trackPath;
            if (TrackData * trackData = loadTrack(trackPath))
            {
                m_tracks.push_back(new Track(trackData));
                numLoaded++;

                MCLogger().info() << "  Found '" << trackPath.toStdString() << "', index="
                    << trackData->index();
            }
            else
            {
                MCLogger().error() << "Couldn't load '" << trackPath.toStdString() << "'..";
            }
        }

        if (!trackPaths.size())
        {
            MCLogger().info() << "  No race tracks found.";
        }
    }

    if (numLoaded)
    {
        setLockedTracks();

        sortTracks();
    }

    return numLoaded;
}

void TrackLoader::setLockedTracks()
{
    // Check if the tracks are locked/unlocked.
    for (Track * track : m_tracks)
    {
        if (!Settings::instance().loadTrackUnlockStatus(*track) &&
            track->trackData().index() > 0) // The first track is never locked
        {
            track->trackData().setIsLocked(true);
        }
        else
        {
            track->trackData().setIsLocked(false);
        }
    }
}

void TrackLoader::sortTracks()
{
    // Sort tracks with respect to their indices
    std::stable_sort(m_tracks.begin(), m_tracks.end(),
        [](Track * lhs, Track * rhs)
        {
             return lhs->trackData().index() < rhs->trackData().index();
        });

    // Cross-link the tracks
    for (unsigned int i = 0; i + 1 < m_tracks.size(); i++)
    {
        m_tracks[i]->setNext(*m_tracks[i + 1]);
        m_tracks[i + 1]->setPrev(*m_tracks[i]);
    }
}

TrackData * TrackLoader::loadTrack(QString path)
{
    QDomDocument doc;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return nullptr;
    }

    if (!doc.setContent(&file))
    {
        file.close();
        return nullptr;
    }

    file.close();

    TrackData * newData = nullptr;

    QDomElement root = doc.documentElement();
    if (root.nodeName() == "track")
    {
        const QString      name     = root.attribute("name",     "undefined");
        const unsigned int cols     = root.attribute("cols",     "0").toUInt();
        const unsigned int rows     = root.attribute("rows",     "0").toUInt();
        const unsigned int lapCount = root.attribute("lapCount", "1").toUInt();
        const unsigned int index    = root.attribute("index",    "999").toUInt();

        if (cols > 0 && rows > 0)
        {
            newData = new TrackData(name, cols, rows);
            newData->setFileName(path);
            newData->setLapCount(lapCount);
            newData->setIndex(index);

            // A temporary route vector.
            std::vector<TargetNodeBase *> route;

            QDomNode node = root.firstChild();
            while(!node.isNull())
            {
                QDomElement element = node.toElement();
                if(!element.isNull())
                {
                    // Read a tile element
                    if (element.nodeName() == "t")
                    {
                        readTile(element, *newData);
                    }
                    // Read an object element
                    else if (element.nodeName() == "o")
                    {
                        readObject(element, *newData);
                    }
                    // Read a target node element
                    else if (element.nodeName() == "n")
                    {
                        readTargetNode(element, *newData, route);
                    }
                }

                node = node.nextSibling();
            }

            newData->route().buildFromVector(route);
        }
    }

    return newData;
}

void TrackLoader::readTile(
    QDomElement & element, TrackData & newData)
{
    const std::string  id           = element.attribute("t", "clear").toStdString();
    const unsigned int computerHint = element.attribute("c", "0").toUInt();

    // X-coordinate in the tile matrix
    unsigned int i = element.attribute("i", "0").toUInt();

    // Y-coordinate in the tile matrix
    unsigned int j = element.attribute("j", "0").toUInt();

    // Orientation angle in degrees.
    int o = element.attribute("o", "0").toInt();

    // Mirror the angle and y-index, because game has the
    // y-axis pointing up.
    o = -o;
    j = newData.map().rows() - 1 - j;

    TrackTile * tile = dynamic_cast<TrackTile *>(newData.map().getTile(i, j));
    assert(tile);

    tile->setRotation(o);
    tile->setTileType(id.c_str());
    tile->setTileTypeEnum(tileTypeEnumFromString(id.c_str()));
    tile->setComputerHint(static_cast<TrackTileBase::ComputerHint>(computerHint));

    // Associate with a surface object corresponging
    // to the tile type.
    // surface() throws if fails. Handled of higher level.
    tile->setSurface(&MCAssetManager::surfaceManager().surface(id));

    // Set preview surface, if found.
    try
    {
        tile->setPreviewSurface(&MCAssetManager::surfaceManager().surface(id + "Preview"));
    }
    catch (...)
    {
        // Don't care
    }
}

TrackTile::TileType TrackLoader::tileTypeEnumFromString(std::string str)
{
    if (str == "corner90")
    {
        return TrackTile::TT_CORNER_90;
    }
    else if (str == "corner45Left")
    {
        return TrackTile::TT_CORNER_45_LEFT;
    }
    else if (str == "corner45Right")
    {
        return TrackTile::TT_CORNER_45_RIGHT;
    }
    else if (str == "straight")
    {
        return TrackTile::TT_STRAIGHT;
    }
    else if (str == "straight45Male")
    {
        return TrackTile::TT_STRAIGHT_45_MALE;
    }
    else if (str == "straight45Female")
    {
        return TrackTile::TT_STRAIGHT_45_FEMALE;
    }
    else if (str == "grass")
    {
        return TrackTile::TT_GRASS;
    }
    else if (str == "sand")
    {
        return TrackTile::TT_SAND;
    }
    else if (str == "sandGrassStraight")
    {
        return TrackTile::TT_SAND_GRASS_STRAIGHT;
    }
    else if (str == "sandGrassStraight45Female")
    {
        return TrackTile::TT_SAND_GRASS_STRAIGHT_45_FEMALE;
    }
    else if (str == "sandGrassCorner")
    {
        return TrackTile::TT_SAND_GRASS_CORNER;
    }
    else if (str == "sandGrassCorner2")
    {
        return TrackTile::TT_SAND_GRASS_CORNER_2;
    }
    else if (str == "finish")
    {
        return TrackTile::TT_FINISH;
    }
    else if (str == "clear")
    {
        return TrackTile::TT_NONE;
    }
    else
    {
        MCLogger().error() << "No mapping for tile '" << str << "'..";
    }

    return TrackTile::TT_NONE;
}

void TrackLoader::readObject(QDomElement & element, TrackData & newData)
{
    const QString role     = element.attribute("r", "");
    const QString category = element.attribute("c", "");

    // X-coordinate in the world
    const int x = element.attribute("x", "0").toInt();

    // Y-coordinate in the world
    const int y = element.attribute("y", "0").toInt();

    // Mirror the angle, because the y-axis is pointing
    // down in the editor's coordinate system.
    const int angle = -element.attribute("o", "0").toInt();

    // Height of the map.
    const int h = newData.map().rows() * TrackTile::TILE_H;

    // The y-coordinates needs to be mirrored, because the y-axis is pointing
    // down in the editor's coordinate system.
    MCVector2dF location(x, h - y);

    newData.objects().add(m_trackObjectFactory.build(category, role, location, angle), true);
}

void TrackLoader::readTargetNode(
    QDomElement & element, TrackData & newData, std::vector<TargetNodeBase *> & route)
{
    const int x = element.attribute("x", "0").toInt();
    const int y = element.attribute("y", "0").toInt();
    const int w = element.attribute("w", "0").toInt();
    const int h = element.attribute("h", "0").toInt();
    const int i = element.attribute("i", "0").toInt();

    // Height of the map. The y-coordinates needs to be mirrored, because
    // the coordinate system is y-wise mirrored in the editor.
    const int mapHeight = newData.map().rows() * TrackTile::TILE_H;

    TargetNodeBase * tnode = new TargetNodeBase;
    tnode->setIndex(i);
    tnode->setLocation(QPointF(x, mapHeight - y));

    if (w > 0 && h > 0)
    {
        tnode->setSize(QSizeF(w, h));
    }

    route.push_back(tnode);
}

unsigned int TrackLoader::tracks() const
{
    return m_tracks.size();
}

Track * TrackLoader::track(unsigned int index) const
{
    if (index < tracks())
    {
        return m_tracks.at(index);
    }

    return nullptr;
}

TrackLoader::~TrackLoader()
{
    for (Track * track : m_tracks)
    {
        delete track;
    }

    m_tracks.clear();
}
