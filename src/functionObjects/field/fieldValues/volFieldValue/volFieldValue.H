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

Class
    Foam::functionObjects::fieldValues::volFieldValue

Description
    Provides a 'fvCellZone' specialisation of the fieldValue function object.

    Given a list of user-specified fields and a 'fvCellZone', a number of
    operations can be performed, such as sums, averages and integrations.

    Example of function object specification:
    \verbatim
    volFieldValue1
    {
        type            volFieldValue;
        libs            ("libfieldFunctionObjects.so");

        log             true;
        writeControl    writeTime;
        writeFields     true;

        cellZone        c0;
        operation       volAverage;

        weightField     alpha1;

        fields
        (
            p
            U
        );
    }
    \endverbatim

Usage
    \table
        Property     | Description                   | Required | Default value
        type         | Type name: volFieldValue      | yes      |
        log          | Write data to standard output | no       | no
        writeFields  | Write the region field values | yes      |
        writeNumberOfCells | Write the number of cells in the zone | no | no
        writeVolume  | Write the volume of the zone  | no       | no
        writeLocation| Write the location (if available) | no   | no
        cellZone     | cellZone                      | yes      |
        operation    | Operation to perform          | yes      |
        weightField  | Name of field to apply weighting | no    | none
        weightFields | Names of fields to apply weighting | no  | none
        fields       | List of fields to operate on  | yes      |
    \endtable

    Where \c cellZone options are:
    \plaintable
        cellZone \<name\>  | Looks-up the named cellZone
        cellZone {type \<zoneGeneratorType\>;...} | \\
            Generates the cellZone locally
        cellZone all       | Selects all cells
    \endplaintable

    The \c operation is one of:
    \plaintable
       none               | No operation
       sum                | Sum
       sumMag             | Sum of component magnitudes
       average            | Ensemble average
       volAverage         | Volume weighted average
       volIntegrate       | Volume integral
       min                | Minimum
       max                | Maximum
       minMag             | Minimum magnitude
       maxMag             | Maximum magnitude
       CoV                | Coefficient of variation: (standard deviation)/mean
       UI                 | Uniformity index: ???
    \endplaintable

See also
    Foam::functionObjects::fieldValues::fieldValue
    Foam::Foam::fvCellZone
    Foam::functionObject

SourceFiles
    volFieldValue.C

\*---------------------------------------------------------------------------*/

#ifndef volFieldValue_functionObject_H
#define volFieldValue_functionObject_H

#include "fieldValue.H"
#include "fvCellZone.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace functionObjects
{
namespace fieldValues
{

/*---------------------------------------------------------------------------*\
                        Class volFieldValue Declaration
\*---------------------------------------------------------------------------*/

class volFieldValue
:
    public fieldValue
{

public:

    // Public data types

        //- Operation type enumeration
        enum class operationType
        {
            none,
            sum,
            sumMag,
            average,
            volAverage,
            volIntegrate,
            min,
            max,
            minMag,
            maxMag,
            CoV,
            UI
        };

        //- Operation type names
        static const NamedEnum<operationType, 12> operationTypeNames_;


    // Public classes

        //- Forward declare the result structure
        template<class Type>
        struct Result;


protected:

    // Protected data

        //- The cellZone to operate on
        fvCellZone zone_;

        //- Operation to apply to values
        operationType operation_;

        //- Weight field names
        wordList weightFieldNames_;

        //- Scale factor
        scalar scaleFactor_;

        //- Optionally write the number of faces in the surface
        const bool writeNCells_;

        //- Optionally write the area of the surface
        const bool writeVolume_;

        //- Optionally write the location if available for this operation
        const bool writeLocation_;


    // Protected Member Functions

        //- Return true if the field name is valid
        template<class Type>
        bool validField(const word& fieldName) const;

        //- Insert field values into values list
        template<class Type>
        tmp<Field<Type>> getFieldValues(const word& fieldName) const;

        //- Apply a comparison operation to the values, returning the limiting
        //  value, its index and processor index
        template<class Op>
        void compareScalars
        (
            const scalarField& values,
            const scalar emptyVal,
            Result<scalar>& result,
            const Op& op
        ) const;

        //- Apply the operation to the values, and return true if successful.
        //  Does nothing unless overloaded below.
        template<class Type, class ResultType>
        bool processValues
        (
            const Field<Type>& values,
            const scalarField& weights,
            const scalarField& V,
            Result<ResultType>& result
        ) const;

        //- Apply Type -> Type operation to the values. Calls
        //  processValuesTypeType.
        template<class Type>
        bool processValues
        (
            const Field<Type>& values,
            const scalarField& weights,
            const scalarField& V,
            Result<Type>& result
        ) const;

        //- Apply Type -> scalar operation to the values
        template<class Type>
        bool processValues
        (
            const Field<Type>& values,
            const scalarField& weights,
            const scalarField& V,
            Result<scalar>& result
        ) const;

        //- Apply scalar -> scalar operation to the values. Calls
        //  processValuesTypeType.
        bool processValues
        (
            const Field<scalar>& values,
            const scalarField& weights,
            const scalarField& V,
            Result<scalar>& result
        ) const;

        //- Apply a Type -> Type operation to the values
        template<class Type>
        bool processValuesTypeType
        (
            const Field<Type>& values,
            const scalarField& weights,
            const scalarField& V,
            Result<Type>& result
        ) const;

        //- Output file header location information for a given type
        template<class Type>
        void writeFileHeaderLocation();

        //- Output file header information
        virtual void writeFileHeader(const label i);


public:

    //- Run-time type information
    TypeName("volFieldValue");


    // Constructors

        //- Construct from name, Time and dictionary
        volFieldValue
        (
            const word& name,
            const Time& runTime,
            const dictionary& dict
        );

        //- Construct from name, objectRegistry and dictionary
        volFieldValue
        (
            const word& name,
            const objectRegistry& obr,
            const dictionary& dict
        );


    //- Destructor
    virtual ~volFieldValue();


    // Public Member Functions

        //- Templated helper function to output field values
        template<class Type>
        bool writeValues
        (
            const word& fieldName,
            const scalarField& weights,
            const scalarField& V
        );

        //- Templated helper function to output field values
        template<class Type, class ResultType>
        bool writeValues
        (
            const word& fieldName,
            const Field<Type>& values,
            const scalarField& weights,
            const scalarField& V
        );

        //- Filter a field according to cellIds
        template<class Type>
        tmp<Field<Type>> filterField(const Field<Type>& field) const;

        //- Read from dictionary
        virtual bool read(const dictionary&);

        //- Calculate and write
        virtual bool write();


        // Mesh changes

            //- Update for mesh motion
            virtual void movePoints(const polyMesh&);

            //- Update topology using the given map
            virtual void topoChange(const polyTopoChangeMap&);

            //- Update from another mesh using the given map
            virtual void mapMesh(const polyMeshMap&);

            //- Redistribute or update using the given distribution map
            virtual void distribute(const polyDistributionMap&);
};


template<>
void volFieldValue::writeFileHeaderLocation<scalar>();


/*---------------------------------------------------------------------------*\
                      Class volFieldValue::Result Declaration
\*---------------------------------------------------------------------------*/

template<class Type>
struct volFieldValue::Result
{
    Type value;
    label celli;
    label proci;
    point cc;
};


template<class Type>
inline Istream& operator>>
(
    Istream& is,
    volFieldValue::Result<Type>& result
)
{
    return is >> result.value >> result.celli >> result.proci >> result.cc;
}


template<class Type>
inline Ostream& operator<<
(
    Ostream& os,
    const volFieldValue::Result<Type>& result
)
{
    return os << result.value << result.celli << result.proci << result.cc;
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace fieldValues
} // End namespace functionObjects
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#ifdef NoRepository
    #include "volFieldValueTemplates.C"
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
