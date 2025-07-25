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

#include "volFieldValue.H"
#include "volFields.H"

// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //

template<class Type>
bool Foam::functionObjects::fieldValues::volFieldValue::validField
(
    const word& fieldName
) const
{
    if
    (
        obr_.foundObject<VolField<Type>>(fieldName)
     || obr_.foundObject<VolInternalField<Type>>(fieldName)
    )
    {
        return true;
    }

    return false;
}


template<class Type>
Foam::tmp<Foam::Field<Type>>
Foam::functionObjects::fieldValues::volFieldValue::getFieldValues
(
    const word& fieldName
) const
{
    if (obr_.foundObject<VolField<Type>>(fieldName))
    {
        return filterField(obr_.lookupObject<VolField<Type>>(fieldName));
    }

    if (obr_.foundObject<VolInternalField<Type>>(fieldName))
    {
        return filterField
        (
            obr_.lookupObject<VolInternalField<Type>>(fieldName)
        );
    }

    FatalErrorInFunction
        << "Field " << fieldName << " not found in database"
        << abort(FatalError);

    return tmp<Field<Type>>(nullptr);
}


template<class Op>
void Foam::functionObjects::fieldValues::volFieldValue::compareScalars
(
    const scalarField& values,
    const scalar emptyVal,
    Result<scalar>& result,
    const Op& op
) const
{
    // Check if the zone on this processor is non-zero sized
    if (values.size())
    {
        label i = 0;
        forAll(values, j)
        {
            if (op(values[j], values[i]))
            {
                i = j;
            }
        }

        result.value = values[i];
        result.celli = zone_.celli(i);
        result.cc = fieldValue::mesh_.C()[result.celli];
    }
    else
    {
        result.value = emptyVal;
    }

    result.proci = Pstream::parRun() ? Pstream::myProcNo() : -1;

    reduce
    (
        result,
        [&op](const Result<scalar>& a, const Result<scalar>& b)
        {
            return op(a.value, b.value) ? a : b;
        }
    );
}


template<class Type, class ResultType>
bool Foam::functionObjects::fieldValues::volFieldValue::processValues
(
    const Field<Type>& values,
    const scalarField& weights,
    const scalarField& V,
    Result<ResultType>& result
) const
{
    return false;
}


template<class Type>
bool Foam::functionObjects::fieldValues::volFieldValue::processValues
(
    const Field<Type>& values,
    const scalarField& weights,
    const scalarField& V,
    Result<Type>& result
) const
{
    return processValuesTypeType(values, weights, V, result);
}


template<class Type>
bool Foam::functionObjects::fieldValues::volFieldValue::processValues
(
    const Field<Type>& values,
    const scalarField& weights,
    const scalarField& V,
    Result<scalar>& result
) const
{
    switch (operation_)
    {
        case operationType::minMag:
        {
            compareScalars(mag(values), vGreat, result, lessOp<scalar>());
            return true;
        }
        case operationType::maxMag:
        {
            compareScalars(mag(values), -vGreat, result, greaterOp<scalar>());
            return true;
        }
        default:
        {
            return false;
        }
    }
}


template<class Type>
bool Foam::functionObjects::fieldValues::volFieldValue::processValuesTypeType
(
    const Field<Type>& values,
    const scalarField& weights,
    const scalarField& V,
    Result<Type>& result
) const
{
    switch (operation_)
    {
        case operationType::sum:
        {
            result.value = gSum(weights*values);
            return true;
        }
        case operationType::sumMag:
        {
            result.value = gSum(cmptMag(values));
            return true;
        }
        case operationType::average:
        {
            result.value = gSum(weights*values)/max(gSum(weights), vSmall);
            return true;
        }
        case operationType::volAverage:
        {
            result.value = gSum(weights*V*values)/max(gSum(weights*V), vSmall);
            return true;
        }
        case operationType::volIntegrate:
        {
            result.value = gSum(weights*V*values);
            return true;
        }
        case operationType::min:
        {
            result.value = gMin(values);
            return true;
        }
        case operationType::max:
        {
            result.value = gMax(values);
            return true;
        }
        case operationType::CoV:
        {
            const Type meanValue = gSum(values*V)/zone_.V();

            const label nComp = pTraits<Type>::nComponents;

            for (direction d=0; d<nComp; ++d)
            {
                const scalarField vals(values.component(d));
                const scalar mean = component(meanValue, d);

                setComponent(result.value, d) =
                    protectedDivide
                    (
                        sqrt(gSum(V*sqr(vals - mean))/zone_.V()),
                        mean
                    );
            }

            return true;
        }
        case operationType::UI:
        {
            const Type meanValue = gSum(values*V)/zone_.V();

            const label nComp = pTraits<Type>::nComponents;

            for (direction d=0; d<nComp; ++d)
            {
                const scalarField vals(values.component(d));
                const scalar mean = component(meanValue, d);

                setComponent(result.value, d) =
                    1 - 0.5*protectedDivide
                    (
                        gSum(V*mag(vals - mean))/zone_.V(),
                        mean
                    );
            }

            return true;
        }
        case operationType::none:
        {
            return true;
        }
        default:
        {
            return false;
        }
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class Type>
bool Foam::functionObjects::fieldValues::volFieldValue::writeValues
(
    const word& fieldName,
    const scalarField& weights,
    const scalarField& V
)
{
    const bool ok = validField<Type>(fieldName);

    if (ok)
    {
        // Get the values
        Field<Type> values(getFieldValues<Type>(fieldName));

        // Write raw values if specified
        if (writeFields_)
        {
            IOField<Type>
            (
                IOobject
                (
                    fieldName + '_' + zone_.name(),
                    time_.name(),
                    obr_,
                    IOobject::NO_READ,
                    IOobject::NO_WRITE
                ),
                (weights*values).ref()
            ).write();
        }

        // Do the operation
        if (operation_ != operationType::none)
        {
            // Apply scale factor
            values *= scaleFactor_;

            bool ok = false;

            #define writeValuesFieldType(fieldType, none)                      \
                ok =                                                           \
                    ok                                                         \
                 || writeValues<Type, fieldType>                               \
                    (                                                          \
                        fieldName,                                             \
                        values,                                                \
                        weights,                                               \
                        V                                                      \
                    );
            FOR_ALL_FIELD_TYPES(writeValuesFieldType);
            #undef writeValuesFieldType

            if (!ok)
            {
                FatalErrorInFunction
                    << "Operation " << operationTypeNames_[operation_]
                    << " not available for values of type "
                    << pTraits<Type>::typeName
                    << exit(FatalError);
            }
        }
    }

    return ok;
}


template<class Type, class ResultType>
bool Foam::functionObjects::fieldValues::volFieldValue::writeValues
(
    const word& fieldName,
    const Field<Type>& values,
    const scalarField& weights,
    const scalarField& V
)
{
    Result<ResultType> result({Zero, -1, -1, point::uniform(NaN)});

    if (processValues(values, weights, V, result))
    {
        // Add to result dictionary, over-writing any previous entry
        resultDict_.add(fieldName, result.value, true);

        if (Pstream::master())
        {
            file() << tab << result.value;

            Log << "    " << operationTypeNames_[operation_]
                << "(" << zone_.name() << ") of " << fieldName
                <<  " = " << result.value;

            if (result.celli != -1)
            {
                Log << " at location " << result.cc;
                if (writeLocation_) file() << tab << result.cc;
            }

            if (result.celli != -1)
            {
                Log << " in cell " << result.celli;
                if (writeLocation_) file() << tab << result.celli;
            }

            if (result.proci != -1)
            {
                Log << " on processor " << result.proci;
                if (writeLocation_) file() << tab << result.proci;
            }

            Log << endl;
        }

        return true;
    }

    return false;
}


template<class Type>
Foam::tmp<Foam::Field<Type>>
Foam::functionObjects::fieldValues::volFieldValue::filterField
(
    const Field<Type>& field
) const
{
    if (zone_.all())
    {
        return field;
    }
    else
    {
        return tmp<Field<Type>>(new Field<Type>(field, zone_.zone()));
    }
}


// ************************************************************************* //
