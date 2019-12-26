// This file is part of Dust Racing 2D.
// Copyright (C) 2015 Jussi Lind <jussi.lind@iki.fi>
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

#include "offtrackdetector.hpp"
#include "car.hpp"
#include "track.hpp"
#include "tracktile.hpp"

#include <MCMathUtil>

#include <cassert>

OffTrackDetector::OffTrackDetector(Car & car)
  : m_car(car)
  , m_track(nullptr)
  , m_tileWLimit(TrackTile::width() / 2 - TrackTile::width() / 10)
  , m_tileHLimit(TrackTile::height() / 2 - TrackTile::height() / 10)
{
}

void OffTrackDetector::setTrack(std::shared_ptr<Track> track)
{
    m_track = track;
}

void OffTrackDetector::update()
{
    assert(m_track);

    {
        const MCVector3dF leftFrontTirePos(m_car.leftFrontTireLocation());
        const auto tile = m_track->trackTileAtLocation(leftFrontTirePos.i(), leftFrontTirePos.j());

        m_car.setLeftSideOffTrack(false);

        if (isOffTrack(leftFrontTirePos, *tile))
        {
            m_car.setLeftSideOffTrack(true);
        }
    }

    {
        const MCVector3dF rightFrontTirePos(m_car.rightFrontTireLocation());
        const auto tile = m_track->trackTileAtLocation(rightFrontTirePos.i(), rightFrontTirePos.j());

        m_car.setRightSideOffTrack(false);

        if (isOffTrack(rightFrontTirePos, *tile))
        {
            m_car.setRightSideOffTrack(true);
        }
    }
}

bool OffTrackDetector::isOffTrack(MCVector2dF tire, const TrackTile & tile) const
{
    if (!tile.hasAsphalt())
    {
        return true;
    }
    else if (
      tile.tileTypeEnum() == TrackTile::TileType::Straight || tile.tileTypeEnum() == TrackTile::TileType::Finish)
    {
        if ((tile.rotation() + 90) % 180 == 0)
        {
            const float y = tire.j();
            if (y > tile.location().y() + m_tileHLimit || y < tile.location().y() - m_tileHLimit)
            {
                return true;
            }
        }
        else if (tile.rotation() % 180 == 0)
        {
            const float x = tire.i();
            if (x > tile.location().x() + m_tileWLimit || x < tile.location().x() - m_tileWLimit)
            {
                return true;
            }
        }
    }
    else if (tile.tileTypeEnum() == TrackTile::TileType::Straight45Male)
    {
        const MCVector2dF diff = tire - MCVector2dF(tile.location().x(), tile.location().y());
        const MCVector2dF rotatedDiff = MCMathUtil::rotatedVector(diff, tile.rotation() - 45);

        if (rotatedDiff.j() > m_tileHLimit || rotatedDiff.j() < -m_tileHLimit)
        {
            return true;
        }
    }
    else if (
      tile.tileTypeEnum() == TrackTile::TileType::Straight45Female)
    {
        const MCVector2dF diff = tire - MCVector2dF(tile.location().x(), tile.location().y());
        const MCVector2dF rotatedDiff = MCMathUtil::rotatedVector(diff, 360 - tile.rotation() - 45);

        if (rotatedDiff.j() < m_tileHLimit)
        {
            return true;
        }
    }

    return false;
}
