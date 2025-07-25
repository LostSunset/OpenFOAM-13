/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2011-2025 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "polyMesh.H"
#include "Time.H"
#include "cellIOList.H"
#include "zonesGenerator.H"
#include "OSspecific.H"

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::polyMesh::setPointsWrite(const Foam::IOobject::writeOption wo)
{
    points_.writeOpt() = wo;

    if (tetBasePtIsPtr_.valid())
    {
        tetBasePtIsPtr_->writeOpt() = wo;
    }
}


void Foam::polyMesh::setTopologyWrite(const Foam::IOobject::writeOption wo)
{
    setPointsWrite(wo);

    faces_.writeOpt() = wo;
    owner_.writeOpt() = wo;
    neighbour_.writeOpt() = wo;
    boundary_.writeOpt() = wo;

    if (pointZones_.noTopoUpdate())
    {
        pointZones_.writeOpt() = wo;
    }

    if (faceZones_.noTopoUpdate())
    {
        faceZones_.writeOpt() = wo;
    }

    if (cellZones_.noTopoUpdate())
    {
        cellZones_.writeOpt() = wo;
    }
}


void Foam::polyMesh::setPointsInstance(const fileName& inst)
{
    if (debug)
    {
        InfoInFunction << "Resetting points instance to " << inst << endl;
    }

    points_.instance() = inst;
    points_.eventNo() = getEvent();

    if (tetBasePtIsPtr_.valid())
    {
        tetBasePtIsPtr_->instance() = inst;
        tetBasePtIsPtr_().eventNo() = getEvent();
    }

    setPointsWrite(IOobject::AUTO_WRITE);
}


void Foam::polyMesh::setInstance(const fileName& inst)
{
    if (debug)
    {
        InfoInFunction << "Resetting topology instance to " << inst << endl;
    }

    setPointsInstance(inst);

    faces_.instance() = inst;
    owner_.instance() = inst;
    neighbour_.instance() = inst;
    boundary_.instance() = inst;

    if (pointZones_.noTopoUpdate())
    {
        pointZones_.instance() = inst;
    }

    if (faceZones_.noTopoUpdate())
    {
        faceZones_.instance() = inst;
    }

    if (cellZones_.noTopoUpdate())
    {
        cellZones_.instance() = inst;
    }

    setTopologyWrite(IOobject::AUTO_WRITE);
}


Foam::polyMesh::readUpdateState Foam::polyMesh::readUpdate()
{
    if (debug)
    {
        InfoInFunction << "Updating mesh based on saved data." << endl;
    }

    polyMesh::readUpdateState state = polyMesh::UNCHANGED;

    // Find the points and faces instance
    const fileName pointsInst(time().findInstance(meshDir(), "points"));
    const fileName facesInst(time().findInstance(meshDir(), "faces"));

    if (debug)
    {
        Info<< "Faces instance: old = " << facesInstance()
            << " new = " << facesInst << nl
            << "Points instance: old = " << pointsInstance()
            << " new = " << pointsInst << endl;
    }

    if (facesInst != facesInstance())
    {
        // Topological change
        if (debug)
        {
            Info<< "Topological change" << endl;
        }

        clearOut();

        points_ = pointIOField
        (
            IOobject
            (
                "points",
                pointsInst,
                meshSubDir,
                *this,
                IOobject::MUST_READ,
                IOobject::NO_WRITE,
                false
            )
        );

        points_.instance() = pointsInst;

        faces_ = faceCompactIOList
        (
            IOobject
            (
                "faces",
                facesInst,
                meshSubDir,
                *this,
                IOobject::MUST_READ,
                IOobject::NO_WRITE,
                false
            )
        );

        faces_.instance() = facesInst;

        owner_ = labelIOList
        (
            IOobject
            (
                "owner",
                facesInst,
                meshSubDir,
                *this,
                IOobject::READ_IF_PRESENT,
                IOobject::NO_WRITE,
                false
            )
        );

        owner_.instance() = facesInst;

        neighbour_ = labelIOList
        (
            IOobject
            (
                "neighbour",
                facesInst,
                meshSubDir,
                *this,
                IOobject::READ_IF_PRESENT,
                IOobject::NO_WRITE,
                false
            )
        );

        neighbour_.instance() = facesInst;

        // Reset the boundary patches
        polyBoundaryMesh newBoundary
        (
            IOobject
            (
                "boundary",
                facesInst,
                meshSubDir,
                *this,
                IOobject::MUST_READ,
                IOobject::NO_WRITE,
                false
            ),
            *this
        );

        // Check that patch types and names are unchanged
        bool boundaryChanged = false;

        if (newBoundary.size() != boundary_.size())
        {
            boundaryChanged = true;
        }
        else
        {
            wordList newTypes = newBoundary.types();
            wordList newNames = newBoundary.names();

            wordList oldTypes = boundary_.types();
            wordList oldNames = boundary_.names();

            forAll(oldTypes, patchi)
            {
                if
                (
                    oldTypes[patchi] != newTypes[patchi]
                 || oldNames[patchi] != newNames[patchi]
                )
                {
                    boundaryChanged = true;
                    break;
                }
            }
        }

        if (boundaryChanged)
        {
            boundary_.clear();
            boundary_.setSize(newBoundary.size());

            forAll(newBoundary, patchi)
            {
                boundary_.set(patchi, newBoundary[patchi].clone(boundary_));
            }
        }
        else
        {
            forAll(boundary_, patchi)
            {
                boundary_[patchi] = polyPatch
                (
                    newBoundary[patchi].name(),
                    newBoundary[patchi].size(),
                    newBoundary[patchi].start(),
                    patchi,
                    boundary_,
                    newBoundary[patchi].type()
                );
            }
        }

        boundary_.instance() = facesInst;


        // Boundary is set so can use initMesh now (uses boundary_ to
        // determine internal and active faces)

        if (!owner_.headerClassName().empty())
        {
            initMesh();
        }
        else
        {
            cellCompactIOList cells
            (
                IOobject
                (
                    "cells",
                    facesInst,
                    meshSubDir,
                    *this,
                    IOobject::MUST_READ,
                    IOobject::NO_WRITE,
                    false
                )
            );

            // Recalculate the owner/neighbour addressing and reset the
            // primitiveMesh
            initMesh(cells);
        }


        // Even if number of patches stayed same still recalculate boundary
        // data.

        // Calculate topology for the patches (processor-processor comms etc.)
        boundary_.topoChange();

        // Calculate the geometry for the patches (transformation tensors etc.)
        boundary_.calcGeometry();

        // Derived info
        bounds_ = boundBox(points_);
        geometricD_ = Zero;
        solutionD_ = Zero;

        // Re-read tet base points
        tetBasePtIsPtr_ = readTetBasePtIs();

        if (boundaryChanged)
        {
            state = polyMesh::TOPO_PATCH_CHANGE;
        }
        else
        {
            state = polyMesh::TOPO_CHANGE;
        }
    }
    else if (pointsInst != pointsInstance())
    {
        // Points moved
        if (debug)
        {
            Info<< "Point motion" << endl;
        }

        clearGeom();


        label nOldPoints = points_.size();

        points_.clear();

        pointIOField newPoints
        (
            IOobject
            (
                "points",
                pointsInst,
                meshSubDir,
                *this,
                IOobject::MUST_READ,
                IOobject::NO_WRITE,
                false
            )
        );

        if (nOldPoints != 0 && nOldPoints != newPoints.size())
        {
            FatalErrorInFunction
                << "Point motion detected but number of points "
                << newPoints.size() << " in "
                << newPoints.objectPath() << " does not correspond to "
                << " current " << nOldPoints
                << exit(FatalError);
        }

        points_.transfer(newPoints);

        points_.instance() = pointsInst;

        // Re-read tet base points
        autoPtr<labelIOList> newTetBasePtIsPtr = readTetBasePtIs();
        if (newTetBasePtIsPtr.valid())
        {
            tetBasePtIsPtr_ = newTetBasePtIsPtr;
        }

        // Calculate the geometry for the patches (transformation tensors etc.)
        boundary_.calcGeometry();

        // Derived info
        bounds_ = boundBox(points_);

        // Rotation can cause direction vector to change
        geometricD_ = Zero;
        solutionD_ = Zero;

        state = polyMesh::POINTS_MOVED;
    }

    // pointZones
    {
        const fileName pointZonesInst
        (
            time().findInstance
            (
                meshDir(),
                "pointZones",
                IOobject::READ_IF_PRESENT,
                facesInst
            )
        );

        if (pointZonesInst != pointZones_.instance())
        {
            pointZoneList newPointZones
            (
                IOobject
                (
                    "pointZones",
                    pointZonesInst,
                    meshSubDir,
                    *this,
                    IOobject::READ_IF_PRESENT,
                    IOobject::NO_WRITE,
                    false
                ),
                *this
            );

            pointZones_.swap(newPointZones);
            pointZones_.instance() = pointZonesInst;
        }
    }

    // faceZones
    {
        const fileName faceZonesInst
        (
            time().findInstance
            (
                meshDir(),
                "faceZones",
                IOobject::READ_IF_PRESENT,
                facesInst
            )
        );

        if (faceZonesInst != faceZones_.instance())
        {
            faceZoneList newFaceZones
            (
                IOobject
                (
                    "faceZones",
                    faceZonesInst,
                    meshSubDir,
                    *this,
                    IOobject::READ_IF_PRESENT,
                    IOobject::NO_WRITE,
                    false
                ),
                *this
            );

            faceZones_.swap(newFaceZones);
            faceZones_.instance() = faceZonesInst;
        }
    }

    // cellZones
    {
        const fileName cellZonesInst
        (
            time().findInstance
            (
                meshDir(),
                "cellZones",
                IOobject::READ_IF_PRESENT,
                facesInst
            )
        );

        if (cellZonesInst != cellZones_.instance())
        {
            cellZoneList newCellZones
            (
                IOobject
                (
                    "cellZones",
                    cellZonesInst,
                    meshSubDir,
                    *this,
                    IOobject::READ_IF_PRESENT,
                    IOobject::NO_WRITE,
                    false
                ),
                *this
            );

            cellZones_.swap(newCellZones);
            cellZones_.instance() = cellZonesInst;
        }
    }

    if (debug && state == polyMesh::UNCHANGED)
    {
        Info<< "No change" << endl;
    }

    return state;
}


bool Foam::polyMesh::writeObject
(
    IOstream::streamFormat fmt,
    IOstream::versionNumber ver,
    IOstream::compressionType cmp,
    const bool write
) const
{
    if (faces_.writeOpt() == AUTO_WRITE)
    {
        auto rmAddressing = [&](const word& name)
        {
            const IOobject faceProcAddressingIO
            (
                name,
                facesInstance(),
                meshSubDir,
                *this
            );

            fileHandler().rm(faceProcAddressingIO.filePath(false));
        };

        if (!Pstream::parRun())
        {
            rmAddressing("cellProc");
        }
        else
        {
            rmAddressing("pointProcAddressing");
            rmAddressing("faceProcAddressing");
            rmAddressing("cellProcAddressing");
        }
    }

    const bool written = objectRegistry::writeObject(fmt, ver, cmp, write);

    const_cast<polyMesh&>(*this).setTopologyWrite(IOobject::NO_WRITE);

    return written;
}


// ************************************************************************* //
