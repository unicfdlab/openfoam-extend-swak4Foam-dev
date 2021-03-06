/*---------------------------------------------------------------------------*\
|                       _    _  _     ___                       | The         |
|     _____      ____ _| | _| || |   / __\__   __ _ _ __ ___    | Swiss       |
|    / __\ \ /\ / / _` | |/ / || |_ / _\/ _ \ / _` | '_ ` _ \   | Army        |
|    \__ \\ V  V / (_| |   <|__   _/ / | (_) | (_| | | | | | |  | Knife       |
|    |___/ \_/\_/ \__,_|_|\_\  |_| \/   \___/ \__,_|_| |_| |_|  | For         |
|                                                               | OpenFOAM    |
-------------------------------------------------------------------------------
License
    This file is part of swak4Foam.

    swak4Foam is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    swak4Foam is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with swak4Foam; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Contributors/Copyright:
    2012-2014, 2016-2018 Bernhard F.W. Gschaider <bgschaid@hfd-research.com>
    2018 Mark Olesen <Mark.Olesen@esi-group.com>

 SWAK Revision: $Id$
\*---------------------------------------------------------------------------*/

#include "MeshesRepository.H"

#include "polyMesh.H"
#include "meshSearch.H"

#include "swak.H"

namespace Foam {

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

defineTypeNameAndDebug(MeshesRepository, 0);

MeshesRepository *MeshesRepository::repositoryInstance(NULL);

MeshesRepository::MeshesRepository()
{
}

// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

MeshesRepository::~MeshesRepository()
{
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

MeshesRepository &MeshesRepository::getRepository()
{
    MeshesRepository*  ptr=repositoryInstance;

    if(debug) {
        Pout << "MeshesRepository: asking for Singleton" << endl;
    }

    if(ptr==NULL) {
        Pout << "swak4Foam: Allocating new repository for sampledMeshes\n";

        ptr=new MeshesRepository();
    }

    repositoryInstance=ptr;

    return *repositoryInstance;
}

fvMesh &MeshesRepository::addMesh(
    const word &name,
    const fileName &caseN,
    const word &region
)
{
    fileName usedN=caseN;
    usedN.expand();

    if(debug) {
        Pout << "MeshesRepository: Adding" << name
            << " (assuming it does not exist)" << endl;
        Pout << "Location: " << usedN << " Region " << region << endl;
    }

    typedef HashPtrTable<Time> tableIterator;

    forAllIter(
        tableIterator,
        times_,
        iter
    ) {
        const Time &time=(*iter());
        fileName meshPath=time.rootPath()/time.caseName();
        word meshRegion=time.dbDir();
        if(meshRegion=="") {
            meshRegion=polyMesh::defaultRegion;
        }
        if(debug) {
            Info << iter.key() << ": " << meshPath << " region: "
                << time.dbDir() << endl;
        }
        if(
            usedN==meshPath
            &&
            region==meshRegion
        ) {
            if(iter.key()==name) {
                if(debug) {
                    Info << "Mesh already present under the name"
                        << name << endl;
                }
                return *meshes_[name];
            }
            FatalErrorIn("MeshesRepository::addMesh")
                << "Mesh of case " << usedN << " region " << region
                    << " is already in the repository under the name "
                    << iter.key()
                    << endl
                    << exit(FatalError);

        }
    }

    if(meshes_.found(name)) {
        FatalErrorIn("MeshesRepository::addMesh")
            << "There is already an entry " << name
                << " in the repository"
                << endl
                << exit(FatalError);

    }

    times_.insert(
        name,
#ifdef FOAM_HASH_PTR_LIST_ACCEPTS_NO_RAW_POINTERS
        autoPtr<Time>(
#endif
            new Time(
                Time::controlDictName,
                usedN.path(),
                fileName(usedN.name())
            )
#ifdef FOAM_HASH_PTR_LIST_ACCEPTS_NO_RAW_POINTERS
        )
#endif
    );
    meshes_.insert(
        name,
#ifdef FOAM_HASH_PTR_LIST_ACCEPTS_NO_RAW_POINTERS
        autoPtr<fvMesh>(
#endif
            new fvMesh(
                IOobject(
                    region,
                    (*times_[name]).timeName(),
                    (*times_[name]),
                    Foam::IOobject::MUST_READ
                )
            )
#ifdef FOAM_HASH_PTR_LIST_ACCEPTS_NO_RAW_POINTERS
        )
#endif
    );

    return *meshes_[name];
}

fvMesh &MeshesRepository::addCoupledMesh(
    const word &name,
    const word &masterName,
    const word &region
)
{
    if(meshes_.found(name)) {
        FatalErrorIn("MeshesRepository::addCoupledMesh")
            << "There is already an entry " << name
                << " in the repository"
                << endl
                << exit(FatalError);
    }
    if(!meshes_.found(masterName)) {
        FatalErrorIn("MeshesRepository::addCoupledMesh")
            << "There is no entry " << masterName
                << " in the repository"
                << endl
#ifdef FOAM_HAS_SORTED_TOC
                << "Available keys:" << nl << meshes_.sortedToc()
#else
                << "Available keys:" << nl << meshes_.toc()
#endif
                << exit(FatalError);
    }

    fvMesh &master=*meshes_[masterName];
    Time &time=const_cast<Time&>(master.time());

    meshes_.insert(
        name,
#ifdef FOAM_HASH_PTR_LIST_ACCEPTS_NO_RAW_POINTERS
        autoPtr<fvMesh>(
#endif
            new fvMesh(
                IOobject(
                    region,
                    time.timeName(),
                    time,
                    Foam::IOobject::MUST_READ
                )
            )
#ifdef FOAM_HASH_PTR_LIST_ACCEPTS_NO_RAW_POINTERS
        )
#endif
    );

    return *meshes_[name];
}

fvMesh &MeshesRepository::getMesh(
    const word &name
)
{
    return *meshes_[name];
}

bool MeshesRepository::hasMesh(
    const word &name
)
{
    return meshes_.found(name);
}

meshToMesh &MeshesRepository::getMeshToMesh(
    const word &name,
    const fvMesh &mesh
)
{
    if(!meshInterpolations_.found(name)) {
         meshInterpolations_.insert(
             name,
#ifdef FOAM_HASH_PTR_LIST_ACCEPTS_NO_RAW_POINTERS
             autoPtr<meshToMesh>(
#endif
                 new meshToMesh(
                     *meshes_[name],
                     mesh
                         #ifdef FOAM_NEW_MESH2MESH
                     ,getInterpolationOrder(name)
                         #endif
                 )
#ifdef FOAM_HASH_PTR_LIST_ACCEPTS_NO_RAW_POINTERS
             )
#endif
         );
    }

    return *meshInterpolations_[name];
}

scalar MeshesRepository::setTime(
    const word &name,
    const string &time,
    label timeIndex
) {
    IStringStream tStream(time);

    return setTime(
        name,
        readScalar(tStream),
        timeIndex
    );
}

scalar MeshesRepository::setTime(
    const word &name,
    scalar time,
    label timeI
) {
    if(debug) {
        Info << "Setting mesh " << name << " to t=" << time << endl;
    }
    fvMesh &mesh=getMesh(name);

    instant iTime=mesh.time().findClosestTime(time);
    if(debug) {
        Info << "Found time " << iTime << " as an approximation for t="
            << time << endl;
    }

    // const_cast<Time&>(mesh.time()).setTime(
    //     iTime,
    //     timeI
    // );

    Time &theTime=*(times_[name]);
    word oldTime=theTime.timeName();

    theTime.setTime(
        iTime,
        timeI
    );

    if(oldTime!=theTime.timeName()) {
        mesh.readUpdate();
        mesh.readModifiedObjects();

#ifdef FOAM_LOOKUPCLASS_NO_CONST
        typedef HashTable<regIOobject*> regIOTable;
#else
        typedef HashTable<const regIOobject*> regIOTable;
#endif

        regIOTable content(mesh.lookupClass<regIOobject>());

        forAllIter(regIOTable,content,iter) {
            word newInstance=theTime.findInstance(
                (*iter)->local(),
                iter.key(),
                IOobject::READ_IF_PRESENT
            );
            if(newInstance==theTime.timeName()) {
                // Deregister the field. Don't know a better way to ensure it is properly reread
                if(debug) {
                    Info << "Deregistering " << iter.key() << endl;
                }
                regIOobject &obj=const_cast<regIOobject&>(*(*iter));
                mesh.polyMesh::checkOut(obj);
            }
        }
   }
    return mesh.time().value();
}

void MeshesRepository::setInterpolationOrder(
    const word &name,
    meshToMeshOrder val
)
{
    interpolationOrder_.set(name,val);
}

meshToMeshOrder MeshesRepository::getInterpolationOrder(
    const word &name
)
{
    if(interpolationOrder_.found(name)) {
        return interpolationOrder_[name];
    } else {
#ifdef FOAM_NEW_MESH2MESH
        return meshToMesh::interpolationMethod::imCellVolumeWeight;
#else
        return meshToMesh::INTERPOLATE;
#endif
    }
}

// * * * * * * * * * * * * * * * Member Operators  * * * * * * * * * * * * * //


// * * * * * * * * * * * * * * * Friend Functions  * * * * * * * * * * * * * //


// * * * * * * * * * * * * * * * Friend Operators  * * * * * * * * * * * * * //

} // namespace

// ************************************************************************* //
