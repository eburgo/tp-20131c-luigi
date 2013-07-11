################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/configNivel.c \
../src/deadlock.c \
../src/procesoNivel.c 

OBJS += \
./src/configNivel.o \
./src/deadlock.o \
./src/procesoNivel.o 

C_DEPS += \
./src/configNivel.d \
./src/deadlock.d \
./src/procesoNivel.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"/home/utnso/git/tp-20131c-luigi/commons" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


