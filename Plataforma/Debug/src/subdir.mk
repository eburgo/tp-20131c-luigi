################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/Orquestador.c \
../src/Planificador.c \
../src/Plataforma.c \
../src/inotify.c 

OBJS += \
./src/Orquestador.o \
./src/Planificador.o \
./src/Plataforma.o \
./src/inotify.o 

C_DEPS += \
./src/Orquestador.d \
./src/Planificador.d \
./src/Plataforma.d \
./src/inotify.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"/home/utnso/git/tp-20131c-luigi/commons" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


